//////////////////////////////////////////////////////////////////////////////////
//@file		IGE_HierarchyWindow.cpp
//@author	Muhammad Izha B Rahim
//@param	Email : izha95\@hotmail.com
//@date		9 SEPT 2019
//@brief	

/*
This window controls the top main menu bar. It also controls the docking
of the editor.
*/
//////////////////////////////////////////////////////////////////////////////////



#include "pch.h"
#include <editor/windows/IGE_HierarchyWindow.h>
#include <editor/commands/CommandList.h>		//Commands
#include <editorstatic/imgui/imgui_internal.h> //InputTextEx
#include <app/Application.h>
#include <scene/SceneManager.h>
#include <core/GameObject.h>
#include <core/Core.h>
#include <IDE.h>		//IDE
#include <iostream>

namespace idk {

	IGE_HierarchyWindow::IGE_HierarchyWindow()
		:IGE_IWindow{ "Hierarchy##IGE_HierarchyWindow",true,ImVec2{ 300,600 },ImVec2{ 150,150 } } {		//Delegate Constructor to set window size
			// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
		// because it would be confusing to have two docking targets within each others.
		window_flags = ImGuiWindowFlags_NoCollapse;




	}

	void IGE_HierarchyWindow::BeginWindow()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2{150.0f,100.0f});

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));


	}
	void IGE_HierarchyWindow::Update()
	{
		
		ImGui::PopStyleVar(2);





		ImGuiStyle& style = ImGui::GetStyle();

		ImGui::PushStyleColor(ImGuiCol_ChildBg, style.Colors[ImGuiCol_TitleBgActive]);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		//Tool bar

		ImGui::SetCursorPos(ImVec2{ 0.0f,ImGui::GetFrameHeight() });


		//Toolbar
		const ImVec2 toolBarSize{ window_size.x, 18.0f };
		const ImGuiWindowFlags childFlags = ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoScrollbar
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoSavedSettings
			| ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoDocking
			| ImGuiWindowFlags_NoCollapse;
		ImGui::BeginChild("HierarchyToolBar", toolBarSize, false, childFlags);
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(3);

		ImGui::SetCursorPosX(5);
		if (ImGui::Button("Create")) {
			ImGui::OpenPopup("CreatePopup");


		}

		if (ImGui::BeginPopup("CreatePopup")) {
			if (ImGui::MenuItem("Create Empty")) {
				IDE& editor = Core::GetSystem<IDE>();
				editor.command_controller.ExecuteCommand(COMMAND(CMD_CreateGameObject));

				auto& sg = Core::GetSystem<SceneManager>().FetchSceneGraph();

				int indent = 0;
				sg.visit([&indent](const auto& handle, int depth) {
					indent += depth;
					for (int i = 0; i < indent; ++i)
						std::cout << ' ';
					std::cout << handle.id << '\n';
				});

			}

			ImGui::EndPopup();
		}


		ImGui::SameLine();
		ImVec2 startPos = ImGui::GetCursorPos();
		ImGui::SetCursorPosX(startPos.x+15);

		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
		static char searchBarChar[512];
		if (ImGui::InputTextEx("##ToolBarSearchBar",NULL, searchBarChar, 512, ImVec2{window_size.x-100,ImGui::GetFrameHeight()-2}, ImGuiInputTextFlags_None)) {
			//Do something
		}
		ImGui::PopStyleVar();

		ImGui::EndChild();

		//Hierarchy Display
		SceneManager& sceneManager = Core::GetSystem<SceneManager>();
		SceneManager::SceneGraph& sceneGraph = sceneManager.FetchSceneGraph();
		
		
		//Refer to TestSystemManager.cpp
		sceneGraph.visit([](const Handle<GameObject>& handle, int depth) {
			if (!handle)
				return;
			for (int i = 0; i < depth; ++i)
				ImGui::Indent();
		//	handle->Transform()->SetParent(parent, true);
			string goName = "GameObject ";
			goName.append(std::to_string(handle.id));
			//ImGui::PushID(&handle);
			if (ImGui::Selectable(goName.c_str())) {
		
			}
		
			//ImGui::PopID();
		});


		//for (auto& i : sceneGraph) {
		//	ImGui::PushID(i.obj.id);
		//
		//	string goName = "GameObject ";
		//	goName.append(std::to_string(i.obj.id));
		//	ImGui::Selectable(goName.c_str());
		//
		//	ImGui::PopID();
		//
		//}

		

	}

}