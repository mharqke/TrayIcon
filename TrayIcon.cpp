

#include "stdafx.h"
#include <assert.h>
#include <cstdio>
#include <cstdlib>

#include "cfg.h"
#include "env.h"
#include "wnd.h"

#include <fstream>
#include <ctime>
#include <queue>
#include <charconv>
#include <Windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <psapi.h>
#include <map>


#include <mutex>

std::mutex m;

HDC hdc;

//using namespace std::filesystem;

#define IDC_START 2001
#define IDC_STOP  2002

#define SWM_TRAYMSG WM_APP//        the message ID sent to our window

#define SWM_SHOW    (WM_APP + 1)//    show the window
#define SWM_HIDE    (WM_APP + 2)//    hide the window
#define SWM_EXIT    (WM_APP + 3)//    close the window

#define UWM_UPDATEINFO    (WM_USER + 4)

CIniConfig config{};

// Global Variables:
HINSTANCE gInstance; // current instance
HANDLE hNewWaitHandle;
NOTIFYICONDATA kData;

HWND kDlg;

const std::string temp_file_name = "D:/3VERGIVEN/common folder/statistic/cpp_app_data/temp.txt";
const std::string main_file_name = "D:/3VERGIVEN/common folder/statistic/cpp_app_data/main.txt";
const std::string log_file_name = "D:/3VERGIVEN/common folder/statistic/cpp_app_data/log.txt";
const std::string focus_file_name = "D:/3VERGIVEN/common folder/statistic/cpp_app_data/focus.txt";
const int TIME_FREQUENSY = 1000; // in milliseconds
const int FOCUSED_FREQUENSY = 500; //in milliseconds


std::stop_source stopSource;

void write_to_file(std::string file_name, auto message, bool overwrite = true) {
	if (overwrite)
	{
		std::ofstream output(file_name);
		output << message;
		output.close();
	}
	else
	{
		std::ofstream output(file_name, std::ios::app);
		output << message;
		output.close();
	}
}


// Forward declarations of functions included in this code module:
static BOOL InitInstance(HINSTANCE, int);
static BOOL OnInitDialog(HWND hWnd);
static void ShowContextMenu(HWND hWnd);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


LPWSTR format_last_error(const std::wstring& msg) {
	std::wostringstream oss;
	const DWORD dwErrorCode = GetLastError();
	WCHAR pBuffer[MAX_PATH];
	DWORD cchMsg = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	                             nullptr,  /* (not used with FORMAT_MESSAGE_FROM_SYSTEM) */
		dwErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		pBuffer,
		MAX_PATH,
	nullptr);

	oss << msg << std::endl << L"(" << dwErrorCode << ")" << " " << pBuffer;
	std::wstring s = oss.str();
	const rsize_t word_count = s.length() + 1;
	wchar_t* p = new wchar_t[word_count];
	wcscpy_s(p, word_count, s.c_str());
	return p;
}

LPWSTR format_process_id(const std::wstring& msg, DWORD process_id) {
	std::wostringstream oss;
	oss << msg << " " << L"(" << GetLastError() << ")" << std::endl;
	std::wstring s = oss.str();
	const rsize_t word_count = s.length() + 1;
	wchar_t* p = new wchar_t[word_count];
	wcscpy_s(p, word_count, s.c_str());
	return p;
}


void InitializeNotificationData()
{

	ZeroMemory(&kData, sizeof(NOTIFYICONDATA));
	kData.cbSize = sizeof(NOTIFYICONDATA);
	kData.uID = 100;
	kData.uFlags = NIF_ICON | NIF_MESSAGE;
	kData.hWnd = kDlg;
	kData.uCallbackMessage = SWM_TRAYMSG;
	kData.uVersion = NOTIFYICON_VERSION_4;
}

void ShowNotificationData(bool on)
{
	NOTIFYICONDATA nid;

	ZeroMemory(&nid, sizeof(nid));
	const std::filesystem::path image_path = on ? CIniConfig::GetOnIconPath() : CIniConfig::GetOffIconPath();

	UINT flags = LR_MONOCHROME;
	flags |= LR_LOADFROMFILE;
	const auto icon = static_cast<HICON>(LoadImage(
		nullptr,
		L"D:/3VERGIVEN/common folder/statistic/cpp_app/favicon.ico",
		IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON),
		LR_LOADFROMFILE));
	nid.hIcon = icon;
	nid.uID = kData.uID;
	nid.hWnd = kData.hWnd;
	nid.uFlags = NIF_ICON;
	Shell_NotifyIcon(NIM_MODIFY, &nid);
	DestroyIcon(icon);
}



class time_app {
public:
	std::string exeFile;
	clock_t timer_for_file;
	clock_t timer_for_focused_apps;
	time_t time_begin;
	time_t time_now;
	std::string time_to_line;
	std::string date_temp;
	std::string time_begin_line;
	std::map<std::string, int> focused_apps_list;

	time_app() {
		temp_to_main();
		time_begin = time(&time_begin);
		time_now = time(&time_now);
		timer_for_file = clock();
		timer_for_focused_apps = clock();
		//temporary variables
		char time_begin_chars[26];
		struct tm buff;

		localtime_s(&buff, &time_begin);
		asctime_s(time_begin_chars, sizeof(time_begin_chars), &buff);
		time_begin_line = time_begin_chars;
		time_begin_line = time_begin_line.substr(0, sizeof(time_begin_line) - 16);

		timer_for_file = clock() - TIME_FREQUENSY *2;
		process();

	}

	~time_app() {
		temp_to_main();
		if (!focused_apps_list.empty()) {
			std::ofstream output(focus_file_name, std::ios::app);
			output << "{\n";
			for (auto i : focused_apps_list) {
				output << i.first << " - " << i.second << '\n';
			}
			output << "}\n";
			output.close();
		}
	}

	void pause() {
		temp_to_main();
	}

	void unpause() {

		//repeat the constractor, +-
		time_t temp_time_begin;
		time_begin = time(&temp_time_begin);

		time_t temp_time_now;
		time_now = time(&temp_time_now);

		char time_begin_chars[26];
		struct tm buff;

		localtime_s(&buff, &time_begin);
		asctime_s(time_begin_chars, sizeof(time_begin_chars), &buff);
		time_begin_line = time_begin_chars;
		time_begin_line = time_begin_line.substr(0, sizeof(time_begin_line) - 16);

		timer_for_file = clock() - TIME_FREQUENSY * 2;
		process();
	}

	void temp_to_main() {

		std::ifstream fin;
		fin.open(temp_file_name);
		if (fin.is_open()) {
			fin.seekg(-1, std::ios_base::end);
			while (1) {

				char ch;
				fin.get(ch);

				if ((int)fin.tellg() <= 1) {
					fin.seekg(0);
					break;
				}
				else if (ch == '\n') {
					break;
				}
				else {
					fin.seekg(-2, std::ios_base::cur);
				}
			}
			std::string lastLine;
			getline(fin, lastLine);
			fin.close();
			write_to_file(main_file_name, lastLine + '\n', 0);
			
			const char* filename = temp_file_name.c_str();
			//std::remove(filename);
		}
		else {
			write_to_file(log_file_name, time_now, 0);
			write_to_file(log_file_name, ": fail to open\n", 0);
		}
	}

	std::string GetFocusedWindowExecutable() {
		HWND hwnd = GetForegroundWindow();
		if (!hwnd) return "";

		static DWORD pid = 0;
		GetWindowThreadProcessId(hwnd, &pid);
		if (pid == 0) return "";

		HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, FALSE, pid);
		if (!hProcess) return "";

		static char exePath[MAX_PATH] = { 0 };
		DWORD len = GetModuleFileNameExA(hProcess, NULL, exePath, MAX_PATH);
		CloseHandle(hProcess);

		if (len == 0) return "";
		return std::string(exePath, len);
	}


	void process() {
		m.lock();
		if (clock() - timer_for_file >= TIME_FREQUENSY) {
			timer_for_file = clock();
			time_now = time(&time_now);
			//temporary variables
			char time_chars[26]; //was static
			struct tm buff;		 //was static
			localtime_s(&buff, &time_now);
			asctime_s(time_chars, sizeof(time_chars), &buff);
			time_to_line = time_chars;
			time_to_line = time_to_line.substr(0, sizeof(time_to_line) - 16);
			date_temp = std::to_string((int)difftime(time_now, time_begin)) + ';' + time_to_line + ';' + time_begin_line;
			write_to_file(temp_file_name, date_temp);
		}
		if (clock() - timer_for_focused_apps >= FOCUSED_FREQUENSY) {
			timer_for_focused_apps = clock();
			std::string exeFile;
			exeFile = GetFocusedWindowExecutable();
			int index = exeFile.find_last_of('\\');
			if (index != -1) exeFile = exeFile.substr(index + 1, exeFile.length());
			if (focused_apps_list.count(exeFile)) {
				focused_apps_list[exeFile]++;
			}
			else {
				focused_apps_list[exeFile] = 1;
			}
			
		}
		m.unlock();
	}

}; //end of class



void collect_data(time_app* app_ptr, std::stop_token stop_token) {
	while (!stop_token.stop_requested()) {
		app_ptr->process();
	}
}



time_app StatApp;
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{


	MSG msg;
	register_class(hInstance, WndProc);
	config.Initialize();

	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}
	BOOL fGotMessage;



	time_app* StatAppPtr = &StatApp;

	std::stop_token stop_token = stopSource.get_token();
	std::thread my_app(collect_data, StatAppPtr, stop_token);

	//show icon in system tray
	ShowNotificationData(true);

	while ((fGotMessage = GetMessage(&msg, (HWND)nullptr, 0, 0)) != 0 && fGotMessage != -1)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}


	unregister_class(hInstance);

	stopSource.request_stop();
	my_app.join();


	return static_cast<int>(msg.wParam);

}

//  Initialize the window and tray icon
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	gInstance = hInstance;

	kDlg = create_window(hInstance, nCmdShow);
	assert(kDlg);
	if (!kDlg)
	{
		LPWSTR msg = format_last_error(L"Create window failed.");
		MessageBox(nullptr, msg, L"ERROR", MB_ICONERROR | MB_OK);
		delete[] msg;
		return FALSE;
	}

	InitializeNotificationData();

	const std::filesystem::path image_path = CIniConfig::GetOffIconPath();

	constexpr UINT flags = LR_LOADFROMFILE;
	const auto icon = static_cast<HICON>(LoadImage(
		nullptr,
		image_path.c_str(),
		IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON),
		flags));
	kData.hIcon = icon;
	Shell_NotifyIcon(NIM_ADD, &kData);
	DestroyIcon(icon);
	kData.hIcon = nullptr;
	//how to open new window 
	//StartProcess();

	return TRUE;
}


BOOL OnInitDialog(HWND hWnd)
{
	const std::filesystem::path image_path = CIniConfig::GetOffIconPath();

	if (const HMENU h_menu = GetSystemMenu(hWnd, FALSE))
	{
		AppendMenu(h_menu, MF_SEPARATOR, 0, nullptr);
	}

	auto hIcon = static_cast<HICON>(LoadImage(
		nullptr,
		image_path.c_str(),
		IMAGE_ICON,
		GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON),
		LR_LOADFROMFILE));
	SendMessage(hWnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hIcon));
	SendMessage(hWnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hIcon));
	DestroyIcon(hIcon);

	return TRUE;
}


void ShowContextMenu(HWND hWnd)
{
	POINT pt;
	GetCursorPos(&pt);

	if (HMENU h_menu = CreatePopupMenu())
	{
		//InsertMenu(h_menu, -1, MF_BYPOSITION, write_to_file, _T("write"));
		//AppendMenu(h_menu, MF_STRING, 0, L"&Disable History");
		if (IsWindowVisible(hWnd))
		{
			InsertMenu(h_menu, -1, MF_BYPOSITION, SWM_HIDE, _T("Hide"));
		}
		else
		{
			InsertMenu(h_menu, -1, MF_BYPOSITION, SWM_SHOW, _T("Show"));
		}

		InsertMenu(h_menu, -1, MF_BYPOSITION, SWM_EXIT, _T("Exit"));

		SetForegroundWindow(hWnd);
		TrackPopupMenu(
			h_menu,
			TPM_BOTTOMALIGN,
			pt.x,
			pt.y,
			0,
			hWnd,
		nullptr);
		DestroyMenu(h_menu);
	}
}

HWND wMsg = nullptr;
void UpdateInfoText(LPCWSTR s)
{
	SetWindowText(wMsg, s);
	delete[] s;
}

void UpdateInfoTextByConst(LPCWSTR s)
{
	SetWindowText(wMsg, s);
}
 
bool flag = true;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wm_id;
	
	if (IsWindowVisible(hWnd) && flag) {
		ShowWindow(hWnd, SW_HIDE);
		flag = false;
	}



	switch (message)
	{
	case SWM_TRAYMSG:
		switch (lParam)
		{
		case WM_LBUTTONDBLCLK:
			ShowWindow(hWnd, SW_HIDE);
			break;

		case WM_RBUTTONDOWN:
		case WM_CONTEXTMENU:
			ShowContextMenu(hWnd);
			break;
		default:
			break;
		}

		break;

	case UWM_UPDATEINFO:
		UpdateInfoText(reinterpret_cast<LPCWSTR>(lParam));
		break;



	case WM_SYSCOMMAND:
		switch (wParam)
		{
		case SC_MINIMIZE:
			return 1;
		
		case SC_MAXIMIZE:
			return 1;
			
		default:
			break;
		}

	case WM_COMMAND:
		wm_id = LOWORD(wParam);

		switch (wm_id)
		{
		case SWM_SHOW:
			m.lock();
			StatApp.pause();
			ShowNotificationData(true);
			ShowWindow(hWnd, SW_RESTORE);

			break;

		case SWM_HIDE:
			m.unlock();
			StatApp.unpause();
			ShowWindow(hWnd, SW_HIDE);
			break;


		case IDC_STOP:
			// if L"stop"
			m.unlock();
			StatApp.unpause();
			ShowWindow(hWnd, SW_HIDE);
			break;

		case SWM_EXIT:
			ShowNotificationData(false);
			kData.uFlags = 0;
			Shell_NotifyIcon(NIM_DELETE, &kData);
			DestroyWindow(hWnd);
			break;

		default:
			break;
		}

		return 1;

	case WM_CREATE:
		CreateWindow(
			L"BUTTON", L"stop", WS_VISIBLE | WS_CHILD,
			0, 0, 400, 200, hWnd, (HMENU)IDC_STOP, 0, NULL);
		OnInitDialog(hWnd);
		break;

	case WM_CLOSE:
		return 0;


	case WM_DESTROY:
		ShowNotificationData(false);
		kData.uFlags = 0;
		Shell_NotifyIcon(NIM_DELETE, &kData);
		PostQuitMessage(0);
		break;

	default:
		break;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}