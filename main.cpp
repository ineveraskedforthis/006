// Dear ImGui: standalone example application for Windows API + DirectX 11

// Learn about Dear ImGui:
// - FAQ				  https://dearimgui.com/faq
// - Getting Started	  https://dearimgui.com/getting-started
// - Documentation		https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui/imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <cstdio>
#include <d3d11.h>
#include <tchar.h>
#include "Urlmon.h"
#include <string>
#include <vector>
#include <winnt.h>
#include <Wininet.h>
#include <fstream>
#include <iostream>
#include <filesystem>

// Data
static ID3D11Device*			g_pd3dDevice = nullptr;
static ID3D11DeviceContext*	 g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*		  g_pSwapChain = nullptr;
static bool					 g_SwapChainOccluded = false;
static UINT					 g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

enum class GameCollection {
	g_004005
};

static std::string url_004 ="https://github.com/ineveraskedforthis/004/releases/download/v001/server.exe";

std::string game_url(GameCollection g) {
	switch (g) {
	case GameCollection::g_004005:
		return url_004;
		break;
	}
	return "";
}

std::string game_download_path(GameCollection g) {
	switch (g) {
	case GameCollection::g_004005:
		return ".\\Downloads\\005\\game";
		break;
	}
	return "";
}

std::string game_download_dir(GameCollection g) {
	switch (g) {
	case GameCollection::g_004005:
		return ".\\Downloads\\005";
		break;
	}
	return "";
}

std::string game_install_dir(GameCollection g) {
		switch (g) {
	case GameCollection::g_004005:
		return ".\\Games\\005\\";
		break;
	}
	return "";
}

static std::ofstream download_stream;
static char download_buffer[1 << 16];
static DWORD download_bytes_last_len;

struct download_order {
	std::string url;
	std::string path;
	DWORD total_bytes;
	bool in_progress;
};

static std::vector<download_order> files_to_download;
int download_left = 0;
int download_right = 0;

static HINTERNET internet = NULL;
static HINTERNET resource = NULL;
static bool installation_in_progress = false;

// Main code
int main(int, char**)
{
	std::filesystem::create_directory(".\\Downloads");
	std::filesystem::create_directory(".\\Games");

	int WIN_WIDTH = 0;
	int WIN_HEIGHT = 0;

	// Make process DPI aware and obtain main monitor scale
	ImGui_ImplWin32_EnableDpiAwareness();
	float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

	// Create application window
	WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Example", nullptr };
	::RegisterClassExW(&wc);
	HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"Game Launcher", WS_OVERLAPPEDWINDOW, 100, 100, (int)(1280 * main_scale), (int)(800 * main_scale), nullptr, nullptr, wc.hInstance, nullptr);

	WIN_WIDTH = (int)(1280 * main_scale);
	WIN_HEIGHT = (int)(800 * main_scale);

	// Initialize Direct3D
	if (!CreateDeviceD3D(hwnd))
	{
		CleanupDeviceD3D();
		::UnregisterClassW(wc.lpszClassName, wc.hInstance);
		return 1;
	}

	// Show the window
	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;	 // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;	  // Enable Gamepad Controls

	// Setup Dear ImGui style
	ImGui::StyleColorsLight();

	// Setup scaling
	ImGuiStyle& style = ImGui::GetStyle();
	style.ScaleAllSizes(main_scale);		// Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
	style.FontScaleDpi = main_scale;		// Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

	// Load Fonts
	// - If fonts are not explicitly loaded, Dear ImGui will call AddFontDefault() to select an embedded font: either AddFontDefaultVector() or AddFontDefaultBitmap().
	//   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small threshold.
	// - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
	// - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code (e.g. use an assertion, display an error and quit).
	// - Read 'docs/FONTS.md' for more instructions and details.
	// - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
	style.FontSizeBase = 20.0f;
	io.Fonts->AddFontFromFileTTF(".\\fonts\\Baskervville-Regular.ttf");
	io.Fonts->AddFontDefaultVector();
	io.Fonts->AddFontDefaultBitmap();
	// ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
	// IM_ASSERT(font != nullptr);

	// Our state
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	// Main loop
	bool done = false;
	while (!done)
	{
		// Poll and handle messages (inputs, window resize, etc.)
		// See the WndProc() function below for our to dispatch events to the Win32 backend.
		MSG msg;
		while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
				done = true;
		}
		if (done)
			break;

		// Handle window being minimized or screen locked
		if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
		{
			::Sleep(10);
			continue;
		}
		g_SwapChainOccluded = false;

		// Handle window resize (we don't resize directly in the WM_SIZE handler)
		if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
		{
			WIN_WIDTH = g_ResizeWidth;
			WIN_HEIGHT = g_ResizeWidth;

			CleanupRenderTarget();
			g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
			g_ResizeWidth = g_ResizeHeight = 0;

			CreateRenderTarget();
		}

		// Start the Dear ImGui frame
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		static GameCollection selected_game = GameCollection::g_004005;

		{
			ImGui::SetNextWindowSize(ImVec2 (400, 400));
			ImGui::SetNextWindowPos(ImVec2 (0, 0));
			ImGui::Begin("List", NULL, ImGuiWindowFlags_NoDecoration);
			if (!installation_in_progress) {
				ImGui::BeginListBox("Games");
				if (ImGui::Selectable("004-005-ARENA", selected_game == GameCollection::g_004005)) {
					selected_game = GameCollection::g_004005;
				}
				ImGui::EndListBox();
			} else {
				ImGui::Text("Installation in progress");
			}
			ImGui::End();
		}

		static bool no_internet = false;
		static bool failed_access_to_resource = false;
		static bool failed_file_creation = false;

		{
			ImGui::SetNextWindowSize(ImVec2 (400, WIN_HEIGHT - 400));
			ImGui::SetNextWindowPos(ImVec2 (0, 0));
			ImGui::Begin("List", NULL, ImGuiWindowFlags_NoDecoration);
			ImGui::Text("Downloads");

			if (no_internet) {
				ImGui::Text("No Internet connection");
			}

			if (failed_access_to_resource) {
				ImGui::Text("Failed access");
			}

			for (int i = download_left; i < download_right; i++) {
				ImGui::Text("Download %s", files_to_download[i].url.c_str());
				ImGui::Text("Bytes: %ld", files_to_download[i].total_bytes);
			}

			if (download_right > download_left && !no_internet) {
				auto& cur = files_to_download[download_left];
				if (internet == NULL) {
					no_internet = false;
					internet = InternetOpenA(
						"Game Launcher",
						INTERNET_OPEN_TYPE_DIRECT,
						NULL,
						NULL,
						0
					);
					if (internet == NULL) {
						no_internet = true;
					}
				} else if (resource == NULL) {
					failed_access_to_resource = false;
					resource = InternetOpenUrlA (
						internet,
						cur.url.c_str(),
						NULL,
						0,
						INTERNET_FLAG_RELOAD,
						0
					);
					if (resource == NULL) {
						failed_access_to_resource = true;
					}
				} else if (!cur.in_progress){
					download_stream.open(cur.path, std::ios::binary);
					cur.in_progress = true;
				} else {
					auto result = InternetReadFile(
						resource,
						download_buffer,
						sizeof(download_buffer),
						&download_bytes_last_len
					);
					if (result && download_bytes_last_len) {
						download_stream.write(download_buffer, download_bytes_last_len);
						cur.total_bytes += download_bytes_last_len;
					} else {
						download_stream.flush();
						download_stream.close();
						InternetCloseHandle(resource);
						resource = NULL;
						download_left++;
					}
				}
			}
			ImGui::End();
		}

		{
			ImGui::SetNextWindowSize(ImVec2 (WIN_WIDTH - 400, WIN_HEIGHT));
			ImGui::SetNextWindowPos(ImVec2 (400, 0));
			ImGui::Begin("Description", NULL, ImGuiWindowFlags_NoDecoration);

			if (selected_game == GameCollection::g_004005) {
				bool love_downloaded = std::filesystem::exists(".\\Downloads\\love.zip") && download_right == download_left;
				bool game_downloaded = std::filesystem::exists(".\\Downloads\\005.zip") && download_right == download_left;

				bool love_extracted = std::filesystem::exists(".\\Tools\\love\\love-11.5-win64\\love.exe");
				bool game_extracted = std::filesystem::exists(".\\Games\\005\\005-main");

				static bool love_extraction_started = false;
				static bool game_extraction_started = false;

				bool ready = love_downloaded && game_downloaded && love_extracted && game_extracted;

				if (!ready && !installation_in_progress) {
					if (ImGui::Button("Install") ) {
						std::filesystem::create_directory(".\\Downloads");
						std::filesystem::create_directory(".\\Tools");
						std::filesystem::create_directory(".\\Games");
						// std::filesystem::create_directory(".\\Tools\\love");
						// std::filesystem::create_directory(".\\Games\\005");

						if (!love_downloaded) {
							download_order love {};
							love.url = "https://github.com/love2d/love/releases/download/11.5/love-11.5-win64.zip";
							love.path = ".\\Downloads\\love.zip";
							love.in_progress = false;
							love.total_bytes = 0;
							files_to_download.push_back(love);
							download_right++;
						}

						if (!game_downloaded) {
							download_order game {};
							game.url = "https://github.com/ineveraskedforthis/005/archive/refs/heads/main.zip";
							game.path = ".\\Downloads\\005.zip";
							game.in_progress = false;
							game.total_bytes = 0;
							files_to_download.push_back(game);
							download_right++;
						}

						installation_in_progress = true;
					}
				} else if (installation_in_progress) {
					if (love_downloaded) {
						ImGui::Text("Love downloaded");
					} else {
						ImGui::Text("Love is not downloaded");
					}
					if (game_downloaded) {
						ImGui::Text("Game downloaded");
					} else {
						ImGui::Text("Game is not downloaded");
					}
					if (love_extracted) {
						ImGui::Text("Love extracted");
					} else {
						ImGui::Text("Love is not extracted");
					}
					if (game_extracted) {
						ImGui::Text("Game extracted");
					} else {
						ImGui::Text("Game is not extracted");
					}

					if (love_downloaded && !love_extracted && !love_extraction_started) {
						love_extraction_started = true;
						system("powershell -command \"Expand-Archive .\\Downloads\\love.zip .\\Tools\\love");
					}
					if (game_downloaded && !game_extracted && !game_extraction_started) {
						game_extraction_started = true;
						system("powershell -command \"Expand-Archive .\\Downloads\\005.zip .\\Games\\005");
					}

					if (ready) {
						installation_in_progress = false;
					}
				} else {
					if (ImGui::Button("Join server 1")) {
						system(".\\Tools\\love\\love-11.5-win64\\love.exe .\\Games\\005\\005-main 90.156.254.148 9090");
					}
				}
			}

			ImGui::End();
		}

		// Rendering
		ImGui::Render();
		const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
		g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
		g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		// Present
		HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
		//HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
		g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
	}

	if (resource != NULL) {
		InternetCloseHandle(resource);
		resource = NULL;
	}

	if (internet != NULL) {
		InternetCloseHandle(internet);
		internet = NULL;
	}

	// Cleanup
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CleanupDeviceD3D();
	::DestroyWindow(hwnd);
	::UnregisterClassW(wc.lpszClassName, wc.hInstance);

	return 0;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
	// Setup swap chain
	// This is a basic setup. Optimally could use e.g. DXGI_SWAP_EFFECT_FLIP_DISCARD and handle fullscreen mode differently. See #8979 for suggestions.
	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	UINT createDeviceFlags = 0;
	//createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	D3D_FEATURE_LEVEL featureLevel;
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
	HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
	if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
		res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
	if (res != S_OK)
		return false;

	CreateRenderTarget();
	return true;
}

void CleanupDeviceD3D()
{
	CleanupRenderTarget();
	if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
	if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
	if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
	ID3D11Texture2D* pBackBuffer;
	g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
	g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
	pBackBuffer->Release();
}

void CleanupRenderTarget()
{
	if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (wParam == SIZE_MINIMIZED)
			return 0;
		g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
		g_ResizeHeight = (UINT)HIWORD(lParam);
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}