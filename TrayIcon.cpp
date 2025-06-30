// Win32 Dialog.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include <assert.h>
#include <cstdio>

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


using namespace std;
using namespace std::filesystem;

#define IDC_START 2001
#define IDC_STOP  2002

#define SWM_TRAYMSG WM_APP//        the message ID sent to our window

#define SWM_SHOW    (WM_APP + 1)//    show the window
#define SWM_HIDE    (WM_APP + 2)//    hide the window
#define SWM_EXIT    (WM_APP + 3)//    close the window

#define UWM_UPDATEINFO    (WM_USER + 4)
#define UWM_CHILDQUIT    (WM_USER + 5)
#define UWM_CHILDCREATE    (WM_USER + 6)

CIniConfig config{};

// Global Variables:
HINSTANCE gInstance; // current instance
HANDLE hJob;
HANDLE hNewWaitHandle;
bool createJob = false;
NOTIFYICONDATA kData;

HWND kDlg;

const string temp_file_name = "D:/3VERGIVEN/common folder/statistic/cpp_app_data/test/temp.txt";
const string main_file_name = "D:/3VERGIVEN/common folder/statistic/cpp_app_data/test/main.txt";
const string log_file_name = "D:/3VERGIVEN/common folder/statistic/cpp_app_data/test/log.txt";
const string test_file_name = "D:/3VERGIVEN/common folder/statistic/cpp_app_data/test/test.txt";
const int FREQUENSY = 1000; // in milliseconds

std::stop_source stopSource;

void write_to_file(string file_name, string message, bool overwrite = true) {
	if (overwrite)
	{
		ofstream output(file_name);
		output << message;
		output.close();
	}
	else
	{
		ofstream output(file_name, std::ios::app);
		output << message;
		output.close();
	}
}



// Forward declarations of functions included in this code module:
static BOOL InitInstance(HINSTANCE, int);
static BOOL OnInitDialog(HWND hWnd);
static void ShowContextMenu(HWND hWnd);

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


LPWSTR format_last_error(const wstring& msg) {
	wostringstream oss;
	const DWORD dwErrorCode = GetLastError();
	WCHAR pBuffer[MAX_PATH];
	DWORD cchMsg = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	                             nullptr,  /* (not used with FORMAT_MESSAGE_FROM_SYSTEM) */
		dwErrorCode,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		pBuffer,
		MAX_PATH,
	nullptr);

	oss << msg << endl << L"(" << dwErrorCode << ")" << " " << pBuffer;
	wstring s = oss.str();
	const rsize_t word_count = s.length() + 1;
	wchar_t* p = new wchar_t[word_count];
	wcscpy_s(p, word_count, s.c_str());
	return p;
}

LPWSTR format_process_id(const wstring& msg, DWORD process_id) {
	wostringstream oss;
	oss << msg << " " << L"(" << GetLastError() << ")" << endl;
	wstring s = oss.str();
	const rsize_t word_count = s.length() + 1;
	wchar_t* p = new wchar_t[word_count];
	wcscpy_s(p, word_count, s.c_str());
	return p;
}


VOID CALLBACK WaitOrTimerCallback(_In_ PVOID lpParameter, _In_ BOOLEAN TimerOrWaitFired)
{
	if (!TimerOrWaitFired)
	{
		[[maybe_unused]] const BOOL succeeded = PostMessage(kDlg, UWM_CHILDQUIT, NULL, NULL);
		assert(succeeded);
	}

	return;
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
	const path image_path = on ? CIniConfig::GetOnIconPath() : CIniConfig::GetOffIconPath();

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

unique_ptr<wchar_t[]> BuildCmdLine()
{
	const path app_path = CIniConfig::GetAppPath();

	const wstring app_args = CIniConfig::GetAppArgs();

	wstringstream wss;
	wss << app_path << " " << app_args;

	wstring s = wss.str();

	const size_t word_count = s.length() + 1;
	unique_ptr<wchar_t[]> cmd_line = make_unique<wchar_t[]>(word_count);
	s.copy(cmd_line.get(), word_count - 1);
	s[word_count - 1] = L'\0';
	return cmd_line;
}

void StartProcess()
{
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	LPWSTR msg;
	ZeroMemory(&si, sizeof(si));
	ZeroMemory(&pi, sizeof(pi));
	BOOL rc;

	HANDLE hJobObject = CreateJobObject(nullptr, nullptr);
	assert(hJobObject != NULL);

	JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli{};
	memset(&jeli, 0, sizeof(JOBOBJECT_EXTENDED_LIMIT_INFORMATION));

	jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	rc = SetInformationJobObject(hJobObject, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
	if (rc == 0)
	{
		// ReSharper disable once CppAssignedValueIsNeverUsed
		rc = CloseHandle(hJobObject);
		assert(rc);

		msg = format_last_error(L"Create Job failed.");
		// ReSharper disable once CppAssignedValueIsNeverUsed
		rc = PostMessage(kDlg, UWM_UPDATEINFO, NULL, reinterpret_cast<LPARAM>(msg));
		assert(rc);
		return;
	}

	si.cb = sizeof(si);
	if (CIniConfig::GetAppHide())
	{
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
	}

	const path startup_dir = CIniConfig::GetWorkDirPath();

	unique_ptr<wchar_t[]>  cmd_line = BuildCmdLine();

	unique_ptr<wchar_t[]> env_block = build_env_block();
	rc = CreateProcess(
		nullptr, // No module name (use command line)
		cmd_line.get(), // Command line
		nullptr, // Process handle not inheritable
		nullptr, // Thread handle not inheritable
		TRUE, // Set handle inheritance to FALSE
		CREATE_UNICODE_ENVIRONMENT, // No creation flags
		env_block.get(), // Use parent's environment block
		startup_dir.c_str(), // Use parent's starting directory
		&si, // Pointer to STARTUPINFO structure
		&pi // Pointer to PROCESS_INFORMATION structure
	);

	if (!rc)
	{
		// ReSharper disable once CppAssignedValueIsNeverUsed
		rc = CloseHandle(hJobObject);
		assert(rc);

		msg = format_last_error(L"CreateProcess failed");
		// ReSharper disable once CppAssignedValueIsNeverUsed
		rc = PostMessage(kDlg, UWM_UPDATEINFO, NULL, reinterpret_cast<LPARAM>(msg));
		assert(rc);
		return;
	}

	rc = AssignProcessToJobObject(hJobObject, pi.hProcess);
	if (!rc)
	{
		msg = format_last_error(L"AssignProcessToJobObject failed.");
		// ReSharper disable once CppAssignedValueIsNeverUsed
		rc = CloseHandle(pi.hProcess);
		assert(rc);
		// ReSharper disable once CppAssignedValueIsNeverUsed
		rc = CloseHandle(hJobObject);
		assert(rc);
		// ReSharper disable once CppAssignedValueIsNeverUsed
		rc = PostMessage(kDlg, UWM_UPDATEINFO, NULL, reinterpret_cast<LPARAM>(msg));
		assert(rc);
		return;
	}

	// ReSharper disable once CppAssignedValueIsNeverUsed
	rc = PostMessage(kDlg, UWM_CHILDCREATE, NULL, reinterpret_cast<LPARAM>(hJobObject));
	assert(rc);

	msg = format_process_id(L"Process Running", pi.dwProcessId);
	// ReSharper disable once CppAssignedValueIsNeverUsed
	rc = PostMessage(kDlg, UWM_UPDATEINFO, NULL, reinterpret_cast<LPARAM>(msg));
	assert(rc);

	// ReSharper disable once CppAssignedValueIsNeverUsed
	rc = RegisterWaitForSingleObject(&hNewWaitHandle, pi.hProcess, WaitOrTimerCallback, hJobObject, INFINITE,
		WT_EXECUTEONLYONCE);
	assert(rc);


}

void StopProcess()
{
	if (hJob)
	{
		// ReSharper disable once CppEntityAssignedButNoRead
		// ReSharper disable once CppJoinDeclarationAndAssignment
		BOOL succeeded;

		// ReSharper disable once CppAssignedValueIsNeverUsed
		succeeded = TerminateJobObject(hJob, -1);
		assert(succeeded);

		// ReSharper disable once CppAssignedValueIsNeverUsed
		succeeded = CloseHandle(hJob);
		assert(succeeded);
	}
}




class time_app {
public:

	clock_t timer1;
	time_t time_begin;
	time_t time_now;
	string time_to_line;
	string date_temp;
	string time_begin_line;

	time_app() {
		temp_to_main();

		time_begin = time(&time_begin);
		time_now = time(&time_now);
		timer1 = clock();
		//temporary variables
		char time_begin_chars[26];
		struct tm buff;

		localtime_s(&buff, &time_begin);
		asctime_s(time_begin_chars, sizeof(time_begin_chars), &buff);
		time_begin_line = time_begin_chars;
		time_begin_line = time_begin_line.substr(0, sizeof(time_begin_line) - 16);

		timer1 = clock() - FREQUENSY*2;
		process();

	}

	~time_app() {
		write_to_file(test_file_name, time_to_line);
	}


	void temp_to_main() {

		ifstream fin;
		fin.open(temp_file_name);
		if (fin.is_open()) {
			fin.seekg(-1, ios_base::end);
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
					fin.seekg(-2, ios_base::cur);
				}
			}
			string lastLine;
			getline(fin, lastLine);
			fin.close();
			write_to_file(main_file_name, lastLine + '\n', 0);
		}
		else {
			write_to_file(log_file_name, time_begin_line + ": fail to open\n", 0);
		}
	}



	void process() {
		if (clock() - timer1 >= FREQUENSY) {
			timer1 = clock();
			time_now = time(&time_now);
			//temporary variables
			static char time_chars[26];
			static struct tm buff;
			localtime_s(&buff, &time_now);
			asctime_s(time_chars, sizeof(time_chars), &buff);
			time_to_line = time_chars;
			time_to_line = time_to_line.substr(0, sizeof(time_to_line) - 16);
			date_temp = to_string((int)difftime(time_now, time_begin)) + ';' + time_to_line + ';' + time_begin_line;
			write_to_file(temp_file_name, date_temp);
		}
	}

};



void collect_data(time_app app, std::stop_token stop_token) {
	while (!stop_token.stop_requested()) {
		app.process();

	}
}



int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
	
	time_app app;

	MSG msg;
	register_class(hInstance, WndProc);
	config.Initialize();


	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}
	BOOL fGotMessage;



	// Main message loop:


	std::stop_token stop_token = stopSource.get_token();
	std::thread my_app(collect_data, app, stop_token);

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

	const path image_path = CIniConfig::GetOffIconPath();

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
	//how to open new window keyword
	//StartProcess();

	return TRUE;
}


BOOL OnInitDialog(HWND hWnd)
{
	const path image_path = CIniConfig::GetOffIconPath();

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

// Name says it all
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
		// note:    must set window to the foreground or the
		//          menu won't disappear when it should
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


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wm_id;

	//if (hJob)
	//{
	//	//ShowWindow(hWnd, SW_HIDE);
	//	CloseHandle(hJob);
	//	hJob = nullptr;
	//}
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

	case UWM_CHILDQUIT:
		if (hJob)
		{
			CloseHandle(hJob);
			hJob = nullptr;
		}
		ShowNotificationData(false);

		UpdateInfoTextByConst(L"Process Exit.");
		break;
	case UWM_CHILDCREATE:
		hJob = reinterpret_cast<HANDLE>(lParam);  // NOLINT(performance-no-int-to-ptr)
		ShowNotificationData(true);
		break;
	case WM_SYSCOMMAND:
		if ((wParam & 0xFFF0) == SC_MINIMIZE)
		{
			ShowWindow(hWnd, SW_HIDE);
			return 1;
		}

		break;

	case WM_COMMAND:
		wm_id = LOWORD(wParam);

		switch (wm_id)
		{
		case SWM_SHOW:

			ShowNotificationData(true);
			ShowWindow(hWnd, SW_RESTORE);
			break;

		case SWM_HIDE:
			ShowWindow(hWnd, SW_HIDE);
			break;

		case IDC_START:
			if (!hJob)
			{
				StartProcess();
			}
			break;

		case IDC_STOP:
			StopProcess();
			hJob = nullptr;
			break;

		case SWM_EXIT:
			DestroyWindow(hWnd);
			break;

		default:
			break;
		}

		return 1;

	case WM_CREATE:
		wMsg = CreateWindow(
			L"STATIC",
			L"Ready to Start",
			WS_VISIBLE | WS_CHILD,
			50, 20, 300, 60,
			hWnd, NULL, 0, NULL);
		CreateWindow(
			L"BUTTON", L"start", WS_VISIBLE | WS_CHILD,
			80, 120, 100, 20, hWnd, (HMENU)IDC_START, 0, NULL);
		CreateWindow(
			L"BUTTON", L"stop", WS_VISIBLE | WS_CHILD,
			200, 120, 100, 20, hWnd, (HMENU)IDC_STOP, 0, NULL);
		OnInitDialog(hWnd);
		break;

	case WM_CLOSE:
		ShowWindow(hWnd, SW_HIDE);
		break;

	case WM_DESTROY:
		ShowNotificationData(false);
		kData.uFlags = 0;
		Shell_NotifyIcon(NIM_DELETE, &kData);
		StopProcess();
		hJob = nullptr;
		PostQuitMessage(0);
		break;

	default:
		break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}