//////////////////////////////////////////////////////////////////////////////////
//@file		AudioSource.cpp
//@author	Muhammad Izha B Rahim
//@param	Email : izha95\@hotmail.com
//@date		08 NOV 2019
//@brief	A GameObject Component that holds AudioClips to play sounds. REFER TO PUSH ID: 6e680f1d FOR BACKUP

//////////////////////////////////////////////////////////////////////////////////


#include <stdafx.h> //Needed for every CPP. Precompiler
#include <audio/AudioSource.h>
#include <res/Resource.h>
#include <file/FileSystem.h>

#include <FMOD/core/fmod.hpp> //FMOD Core
#include <FMOD/core/fmod_errors.h> //ErrorString
#include <audio/AudioSystem.h> //AudioSystem
#include <res/ResourceHandle.inl>
#include <res/ResourceManager.inl>
#include <res/ResourceHandle.inl>

#include <core/GameObject.inl>
#include <common/Transform.h>

#include <ds/result.inl>

namespace idk
{
	AudioSource::~AudioSource()
	{
		StopAll();

	}
	void AudioSource::Play(int index)
	{
		if (audio_clip_list.size() > index) { //Check if it is in array
			if (!audio_clip_list[index])
				return;
			AudioSystem& audioSystem = Core::GetSystem<AudioSystem>();
			const FMOD_MODE currentMode = ConvertSettingToFMOD_MODE();
			ResizeAudioClipListData();

			FMOD::ChannelGroup* outputChannel = nullptr;
			switch (soundGroup) {
				case 1:
					outputChannel = audioSystem._channelGroup_SFX;
					break;
				case 2:
					outputChannel = audioSystem._channelGroup_MUSIC;
					break;
				case 3:
					outputChannel = audioSystem._channelGroup_AMBIENT;
					break;
				case 4:
					outputChannel = audioSystem._channelGroup_DIALOGUE;
					break;
			}



			audio_clip_list[index]->_soundHandle->setMode(currentMode);
			audioSystem.ParseFMOD_RESULT(audioSystem._Core_System->playSound(audio_clip_list[index]->_soundHandle, outputChannel, false, &audio_clip_channels[index])); //Creates a channel for audio to use. Start as paused to edit stuff first.
			audio_clip_channels[index]->setPriority(priority);

		}
	}
	void AudioSource::PlayAll()
	{
		AudioSystem& audioSystem = Core::GetSystem<AudioSystem>();
		const FMOD_MODE currentMode = ConvertSettingToFMOD_MODE();
		ResizeAudioClipListData();

		FMOD::ChannelGroup* outputChannel = nullptr;
		switch (soundGroup) {
		case 1:
			outputChannel = audioSystem._channelGroup_SFX;
			break;
		case 2:
			outputChannel = audioSystem._channelGroup_MUSIC;
			break;
		case 3:
			outputChannel = audioSystem._channelGroup_AMBIENT;
			break;
		case 4:
			outputChannel = audioSystem._channelGroup_DIALOGUE;
			break;
		}


		for (int i = 0; i < audio_clip_list.size(); ++i) {
			audio_clip_list[i]->_soundHandle->setMode(currentMode);
			audioSystem.ParseFMOD_RESULT(audioSystem._Core_System->playSound(audio_clip_list[i]->_soundHandle, outputChannel, false, &audio_clip_channels[i])); //Creates a channel for audio to use. Start as paused to edit stuff first.
			audio_clip_channels[i]->setPriority(priority);
		}
	}
	void AudioSource::Stop(int index)
	{
		if (audio_clip_list.size() > index) { //Check if it is in array
			ResizeAudioClipListData();

			if (audio_clip_channels[index] != nullptr) {
				AudioSystem& audioSystem = Core::GetSystem<AudioSystem>();
				bool checkIsPlaying = false;	//An invalid channel can still return an isplaying, use this to stop!
				const FMOD_RESULT result = audio_clip_channels[index]->isPlaying(&checkIsPlaying);
				if (result == FMOD_OK && checkIsPlaying)
					audioSystem.ParseFMOD_RESULT(audio_clip_channels[index]->stop());
				audio_clip_channels[index] = nullptr;
			}
		}
	}
	void AudioSource::StopAll()
	{
		ResizeAudioClipListData();


		for (int i = 0; i < audio_clip_channels.size(); ++i) {
			if (audio_clip_channels[i] != nullptr) {
				audio_clip_channels[i]->stop(); //Ignore any FMOD_RESULT errors as the channel is to be nulled anyway

				audio_clip_channels[i] = nullptr;
			}
		}

	}

	int AudioSource::AddAudioClip(string_view filePath)
	{
		auto audioPtr1 = Core::GetResourceManager().Load<AudioClip>(PathHandle(filePath));
		if (audioPtr1) {
			audio_clip_list.emplace_back(*audioPtr1);
			ResizeAudioClipListData();
			return static_cast<int>(audio_clip_list.size() - 1);
		}
		else
			return -1;
	}


	void AudioSource::RemoveAudioClip(int index)
	{
		if (audio_clip_list.size() > index) { //Check if it is in array
			Stop(index); //Stop first
			audio_clip_list.erase(audio_clip_list.begin() + index);
			audio_clip_channels.erase(audio_clip_channels.begin() + index);
			audio_clip_volume.erase(audio_clip_volume.begin() + index);

		}
	}
	bool AudioSource::IsAudioClipPlaying(int index)
	{
		bool returnVal = false;

		if (audio_clip_channels.size() > index) { //Check if it is in array
			//Copied from AudioClip
			if (audio_clip_channels[index] == nullptr)
				return returnVal;

			audio_clip_channels[index]->isPlaying(&returnVal);
		}
		return returnVal;
	}
	bool AudioSource::IsAnyAudioClipPlaying()
	{
		bool returnVal = false;
		for (int i = 0; i < audio_clip_channels.size(); ++i) {
			audio_clip_channels[i]->isPlaying(&returnVal);
			if (returnVal)
				return true;
		}
		return false;
	}
	void AudioSource::UpdateAudioClips()
	{
		const FMOD_MODE mode = ConvertSettingToFMOD_MODE();


		const auto audio_clip_list_size = audio_clip_list.size();
		const auto audio_clip_channels_size = audio_clip_channels.size(); //Channel is updated on Play to reduce calls.

		if (audio_clip_list_size == audio_clip_channels_size) { //Only do when channel is of correct size!
			for (int i = 0; i < audio_clip_list_size; ++i) {
				if (!audio_clip_list[i])
					continue;
				//---UPDATE CHANNEL---
				if (audio_clip_channels[i] != nullptr) {
					bool isPlaying;
					const FMOD_RESULT result = audio_clip_channels[i]->isPlaying(&isPlaying);
					if (result != FMOD_OK || !isPlaying) {
						audio_clip_channels[i] = nullptr; //Forget the channel once it is done playing
						//printf("Channel finished playing\n");
						continue; //Go to next audio
					}
				}
				else
					continue;


				FMOD_RES(audio_clip_channels[i]->setVolume(volume * audio_clip_volume[i]));
				pitch = pitch < 0 ? 0 : pitch;
				FMOD_RES(audio_clip_channels[i]->setPitch(pitch));
				FMOD_RES(audio_clip_channels[i]->setMode(mode));

				//Update Priority


				//Update Position
				//If the sound is not unique, the position will not be updated when the sound is replayed.
				if (is3Dsound) {
					if (const Handle<GameObject> go = GetGameObject()) {
						if (const Handle<Transform> transform = go->GetComponent<Transform>()) {
							const auto globalPos = transform->GlobalPosition();
							FMOD_VECTOR position = { globalPos.x,globalPos.y ,globalPos.z };
							FMOD_VECTOR vel = {  }; //Disable doppler effect
							FMOD_RES(audio_clip_channels[i]->set3DMinMaxDistance(minDistance, maxDistance));
							FMOD_RES(audio_clip_channels[i]->set3DAttributes(&position, &vel));
						}
					}
				}
			}
		}
	}
	void AudioSource::RefreshSoundGroups()
	{
		AudioSystem& audioSystem = Core::GetSystem<AudioSystem>();
		FMOD::ChannelGroup* outputChannel = nullptr;
		switch (soundGroup) {
		case 1:
			outputChannel = audioSystem._channelGroup_SFX;
			break;
		case 2:
			outputChannel = audioSystem._channelGroup_MUSIC;
			break;
		case 3:
			outputChannel = audioSystem._channelGroup_AMBIENT;
			break;
		case 4:
			outputChannel = audioSystem._channelGroup_DIALOGUE;
			break;
		}


		for (int i = 0; i < audio_clip_channels.size(); ++i) {
			audio_clip_channels[i]->setChannelGroup(outputChannel);
		}
	}
	int AudioSource::FindAudio(string_view name)
	{
		for (int i = 0; i < audio_clip_list.size(); ++i) {
			const auto stringName = audio_clip_list[i]->GetName();
			if (stringName == name)
				return i;
		}
		return -1;
	}

	FMOD_MODE AudioSource::ConvertSettingToFMOD_MODE()
	{
		FMOD_MODE output = FMOD_DEFAULT | FMOD_3D_LINEARROLLOFF;
		output |= isLoop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF;
		output |= is3Dsound ? FMOD_3D : FMOD_2D;
		if (isUnique) {
			output |= FMOD_UNIQUE;
		}
		return output;
	}

	void AudioSource::FMOD_RES(FMOD_RESULT e) //Same as audiosystem ParseFMOD_RESULT, but does not update the _result variable in audiosystem
	{
		AudioSystem& audioSystem = Core::GetSystem<AudioSystem>();
		audioSystem.ParseFMOD_RESULT_2(e);
	}

	void AudioSource::ResizeAudioClipListData()
	{
		audio_clip_channels.resize(audio_clip_list.size(), nullptr);
		audio_clip_volume.resize(audio_clip_list.size(), 1);
	}
}