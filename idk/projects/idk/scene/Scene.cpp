#include "stdafx.h"
#include "Scene.h"
#include <scene/SceneManager.h>
#include <core/GameObject.inl>
#include <util/ioutils.h>
#include <res/ResourceManager.inl>
#include <res/ResourceHandle.inl>
#include <file/FileSystem.h>
#include <serialize/text.h>
#include <ds/span.inl>

namespace idk
{
	Scene::Scene(unsigned char index)
		: scene_id{ index }
	{
	}
	Handle<GameObject> Scene::CreateGameObject(const Handle<GameObject>& handle)
	{
		return GameState::GetGameState().CreateObject<GameObject>(handle);
	}

	Handle<GameObject> Scene::CreateGameObject()
	{
		return GameState::GetGameState().CreateObject<GameObject>(scene_id);
	}

	void Scene::DestroyGameObject(const Handle<GameObject>& go)
	{
		GameState::GetGameState().DestroyObject(go);
	}

	bool Scene::Loaded()
	{
		return _loaded;
	}

	SceneLoadResult Scene::LoadFromResourcePath()
	{
		auto res = Activate();
		if (res != SceneLoadResult::Ok)
			return res;

		auto path = Core::GetResourceManager().GetPath(GetHandle());
		if (!path)
			return SceneLoadResult::Err_ScenePathNotFound;

		auto stream = Core::GetSystem<FileSystem>().Open(*path, FS_PERMISSIONS::READ);
		parse_text(stringify(stream), *this);
		return SceneLoadResult::Ok;
	}

	SceneLoadResult Scene::Activate()
	{
		if (_loaded)
			return SceneLoadResult::Err_SceneAlreadyActive;

		_loaded = true;
		GameState::GetGameState().ActivateScene(scene_id);
		Core::GetSystem<SceneManager>()._scenes[scene_id] = GetHandle();
		return SceneLoadResult::Ok;
	}

	SceneUnloadResult Scene::Deactivate()
	{
		if (!_loaded)
			return SceneUnloadResult::Err_SceneAlreadyInactive;

		Core::GetSystem<SceneManager>()._scenes[scene_id] = RscHandle<Scene>{};
		GameState::GetGameState().DeactivateScene(scene_id);
		_loaded = false;

		return SceneUnloadResult::Ok;
	}

	Scene::iterator Scene::begin() const
	{
		const auto span = GameState::GetGameState().GetObjectsOfType<GameObject>();
		auto beg = span.begin();
		const auto etr = span.end();

		while (beg != etr && beg->GetHandle().scene != scene_id)
			++beg; 

		return iterator{scene_id, beg, etr};
	}

	Scene::iterator Scene::end() const
	{
		const auto span = GameState::GetGameState().GetObjectsOfType<GameObject>();
		return iterator{scene_id, span.end(), span.end() };
	}

	GameObject& Scene::iterator::operator*()
	{
		return *curr_;
	}

	GameObject* Scene::iterator::operator->()
	{
		return curr_;
	}

	Scene::iterator& Scene::iterator::operator++()
	{
		do
		{
			++curr_;
		} while (curr_ != end_ && curr_->GetHandle().scene != scene_id_);

		return *this;
	}

	Scene::iterator  Scene::iterator::operator++(int)
	{
		auto copy = *this;
		operator++();
		return copy;
	}

	bool Scene::iterator::operator==(const iterator& rhs) const
	{
		return curr_ == rhs.curr_;
	}

	bool Scene::iterator::operator!=(const iterator& rhs) const
	{
		return curr_ != rhs.curr_;
	}

	Scene::iterator::iterator(uint8_t scene_id, GameObject* curr, GameObject* end)
		: curr_{curr}, end_{end}, scene_id_{scene_id}
	{
	}
}