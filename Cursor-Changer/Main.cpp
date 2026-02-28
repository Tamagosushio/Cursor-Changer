# include <Siv3D.hpp>
# include <Windows.h>

namespace CursorChanger
{
	/// @brief レジストリキーに定義されたカーソルスキーム名を std::wstring の配列として返す
	Array<String> GetAvailableSchemes()
	{
		Array<String> schemes;
		HKEY hkey;
		// レジストリキーを開く
		if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Cursors\\Schemes", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
		{
			wchar_t value_name[256];
			DWORD value_name_size = 256;
			DWORD index = 0;
			// レジストリキーの値を列挙
			while (RegEnumValueW(hkey, index, value_name, &value_name_size, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
			{
				schemes.push_back(Unicode::FromWstring(value_name));
				index++;
				value_name_size = 256;
			}
			RegCloseKey(hkey);
		}
		return schemes;
	}

	/// @brief 現在のカーソルデザインのインデックスを取得する
	Optional<size_t> GetCurrentSchemeIndex(const Array<String>& schemes)
	{
		HKEY hkey;
		if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Cursors", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
		{
			wchar_t current_scheme[256] = { 0 };
			DWORD buffer_size = sizeof(current_scheme);
			if (RegQueryValueExW(hkey, nullptr, nullptr, nullptr, reinterpret_cast<LPBYTE>(current_scheme), &buffer_size) == ERROR_SUCCESS)
			{
				String current_scheme_str = Unicode::FromWstring(current_scheme);
				for (size_t i = 0; i < schemes.size(); i++)
				{
					if (schemes[i] == current_scheme_str)
					{
						RegCloseKey(hkey);
						return i;
					}
				}
			}
			RegCloseKey(hkey);
		}
		return none;
	}

	// キー名リスト
	const Array<String> cursor_names = {
		U"Arrow",
		U"Help",
		U"AppStarting",
		U"Wait",
		U"Crosshair",
		U"IBeam",
		U"NWPen",
		U"No",
		U"SizeNS",
		U"SizeWE",
		U"SizeNWSE",
		U"SizeNESW",
		U"SizeAll",
		U"UpArrow",
		U"Hand",
		U"Pin",
		U"Person"
	};

	/// @brief デザイン名のカーソルスキームを設定する
	void ApplyScheme(const String& scheme_name)
	{
		HKEY hkey_schemes;
		HKEY hkey_cursors;
		std::wstring scheme_name_w = scheme_name.toWstr();
		// レジストリキーを開く
		if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Cursors\\Schemes", 0, KEY_READ, &hkey_schemes) == ERROR_SUCCESS)
		{
			wchar_t paths_buffer[2048] = { 0 };
			DWORD buffer_size = sizeof(paths_buffer);
			// カンマ区切りのカーソルパス群を取得
			if (RegQueryValueExW(hkey_schemes, scheme_name_w.c_str(), nullptr, nullptr, reinterpret_cast<LPBYTE>(paths_buffer), &buffer_size) == ERROR_SUCCESS)
			{
				// カーソルスキームのパスをレジストリキーに設定
				if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Control Panel\\Cursors", 0, KEY_SET_VALUE, &hkey_cursors) == ERROR_SUCCESS)
				{
					String paths_str = Unicode::FromWstring(paths_buffer);
					Array<String> paths = paths_str.split(U',');
					for (size_t i = 0; i < paths.size() && i < cursor_names.size(); i++)
					{
						if (paths[i].isEmpty())
						{
							// 空文字列をセット
							RegSetValueExW(hkey_cursors, cursor_names[i].toWstr().c_str(), 0, REG_EXPAND_SZ, reinterpret_cast<const BYTE*>(L""), sizeof(wchar_t));
						}
						wchar_t expanded_path[MAX_PATH];
						// 環境変数を絶対パスにに変換
						ExpandEnvironmentStringsW(paths[i].toWstr().c_str(), expanded_path, MAX_PATH);
						// カーソルのパスをレジストリキーに設定
						RegSetValueExW(hkey_cursors, cursor_names[i].toWstr().c_str(), 0, REG_EXPAND_SZ, reinterpret_cast<const BYTE*>(expanded_path), (wcslen(expanded_path) + 1) * sizeof(wchar_t));
					}
					// 現在のデザインを更新
					RegSetValueExW(hkey_cursors, nullptr, 0, REG_SZ, reinterpret_cast<const BYTE*>(scheme_name_w.c_str()), (scheme_name_w.length() + 1) * sizeof(wchar_t));
					RegCloseKey(hkey_cursors);
					// システムに変更を通知
					SystemParametersInfoW(SPI_SETCURSORS, 0, nullptr, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
				}
			}
			RegCloseKey(hkey_schemes);
		}
	}

}

namespace TaskTray
{
	const UINT kTrayIconMessage = WM_USER + 1;
	const UINT kMenuExitId = 1001;
	WNDPROC original_window_proc = nullptr;

	LRESULT CALLBACK CustomWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
	{
		if (message == kTrayIconMessage)
		{
			// 左クリックでウィンドウの再表示
			if (LOWORD(lparam) == WM_LBUTTONUP)
			{
				ShowWindow(hwnd, SW_RESTORE);
				SetForegroundWindow(hwnd);
			}
			// 右クリックでコンテキストメニューの表示
			else if (LOWORD(lparam) == WM_RBUTTONUP)
			{
				// カーソルの位置を取得
				POINT cursor_pos;
				GetCursorPos(&cursor_pos);
				// 空のポップアップメニューを作成
				HMENU menu = CreatePopupMenu();
				// 終了項目を追加
				AppendMenuW(menu, MF_STRING, kMenuExitId, L"Exit");
				// メニューを表示
				SetForegroundWindow(hwnd);
				// メニューの選択を待ち、選択された項目のIDを取得
				int selected_id = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, cursor_pos.x, cursor_pos.y, 0, hwnd, nullptr);
				// メニューを破棄
				DestroyMenu(menu);
				/// 終了項目が選択された場合はアプリケーションを終了
				if (selected_id == kMenuExitId)
				{
					System::Exit();
				}
			}
			return 0;
		}
		return CallWindowProcW(original_window_proc, hwnd, message, wparam, lparam);
	}

	void RegisterTrayIcon(HWND hwnd)
	{
		// タスクトレイアイコンの情報を設定
		NOTIFYICONDATAW notify_icon_data = {
			.cbSize = sizeof(NOTIFYICONDATAW), // 構造体のサイズ
			.hWnd = hwnd, // アイコンを表示するウィンドウのハンドル
			.uID = 1, // アイコンの識別子
			.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP, // アイコン、メッセージ、ツールチップを使用することを指定
			.uCallbackMessage = kTrayIconMessage, // アイコンに対するイベントを受け取るためのメッセージID
			.hIcon = static_cast<HICON>(LoadImageW(nullptr, L"icon.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE)), // アイコンのハンドルを指定
		};
		// ツールチップテキストを設定
		wcscpy_s(notify_icon_data.szTip, L"Cursor Changer");
		// タスクトレイにアイコンを追加
		Shell_NotifyIconW(NIM_ADD, &notify_icon_data);
	}
}

void Main()
{
	// Siv3Dのウィンドウハンドルを取得
	HWND hwnd = static_cast<HWND>(Platform::Windows::Window::GetHWND());
	// ウィンドウをサブクラス化を実行し、独自のプロシージャを登録
	TaskTray::original_window_proc = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(TaskTray::CustomWindowProc)));
	// タスクトレイアイコンの登録
	TaskTray::RegisterTrayIcon(hwnd);
	// 閉じるボタンで終了しないように変更
	System::SetTerminationTriggers(System::GetTerminationTriggers() & ~UserAction::CloseButtonClicked);

	Array<String> schemes = CursorChanger::GetAvailableSchemes();
	ListBoxState listbox_state{ schemes };
	listbox_state.selectedItemIndex = CursorChanger::GetCurrentSchemeIndex(schemes);

	while (System::Update())
	{
		// 閉じるボタンが押された時
		if (System::GetUserActions() & UserAction::CloseButtonClicked)
		{
			// ウィンドウを非表示にする
			ShowWindow(hwnd, SW_HIDE);
		}

		if (SimpleGUI::Button(U"終了", Vec2{ 20, 20 }))
		{
			System::Exit();
		}

		if (SimpleGUI::ListBox(listbox_state, Vec2{ 20,70 }, 300, 200))
		{
			if (listbox_state.selectedItemIndex)
			{
				String selected_scheme = schemes[*listbox_state.selectedItemIndex];
				CursorChanger::ApplyScheme(selected_scheme);
			}
		}
	}
}
