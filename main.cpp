#include <algorithm>
#include <tchar.h>
#include <windows.h>

#include <memory>
#include <filesystem>
#include <chrono>
#include <string>
#include <vector>

#include "robin_hood.h"


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#define WAIT_TIME (50)


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#ifdef UNICODE
PWCHAR* CommandLineToArgvT(PWCHAR CmdLine, size_t* _argc)
{
	PWCHAR* argv;
	PWCHAR _argv;
	size_t len;
	size_t argc;
	WCHAR a;
	size_t i, j;

	bool in_QM;
	bool in_TEXT;
	bool in_SPACE;

	len = wcslen(CmdLine);
	i = ((len + 2) / 2) * sizeof(PVOID) + sizeof(PVOID);

	argv = (PWCHAR*)GlobalAlloc(GMEM_FIXED, i + (len + 2) * sizeof(WCHAR));

	_argv = (PWCHAR)(((PUCHAR)argv) + i);

	argc = 0;
	argv[argc] = _argv;
	in_QM = false;
	in_TEXT = false;
	in_SPACE = true;
	i = 0;
	j = 0;

	while (a = CmdLine[i])
	{
		if (in_QM)
		{
			if (a == L'\"')
			{
				in_QM = false;
			}
			else
			{
				_argv[j] = a;
				++j;
			}
		}
		else
		{
			switch (a)
			{
			case L'\"':
				in_QM = true;
				in_TEXT = true;
				if (in_SPACE)
				{
					argv[argc] = _argv + j;
					++argc;
				}
				in_SPACE = false;
				break;
			case L' ':
			case L'\t':
			case L'\n':
			case L'\r':
				if (in_TEXT)
				{
					_argv[j] = L'\0';
					++j;
				}
				in_TEXT = false;
				in_SPACE = true;
				break;
			default:
				in_TEXT = true;
				if (in_SPACE)
				{
					argv[argc] = _argv + j;
					++argc;
				}
				_argv[j] = a;
				++j;
				in_SPACE = false;
				break;
			}
		}
		++i;
	}
	_argv[j] = L'\0';
	argv[argc] = nullptr;

	(*_argc) = argc;
	return argv;
}

#define ToTString std::to_wstring
#else
PCHAR* CommandLineToArgvT(PCHAR CmdLine, size_t* _argc)
{
	PCHAR* argv;
	PCHAR _argv;
	size_t len;
	size_t argc;
	CHAR a;
	size_t i, j;

	bool in_QM;
	bool in_TEXT;
	bool in_SPACE;

	len = strlen(CmdLine);
	i = ((len + 2) / 2) * sizeof(PVOID) + sizeof(PVOID);

	argv = (PCHAR*)GlobalAlloc(GMEM_FIXED, i + (len + 2) * sizeof(CHAR));

	_argv = (PCHAR)(((PUCHAR)argv) + i);

	argc = 0;
	argv[argc] = _argv;
	in_QM = false;
	in_TEXT = false;
	in_SPACE = true;
	i = 0;
	j = 0;

	while (a = CmdLine[i])
	{
		if (in_QM)
		{
			if (a == '\"')
			{
				in_QM = false;
			}
			else
			{
				_argv[j] = a;
				++j;
			}
		}
		else
		{
			switch (a)
			{
			case '\"':
				in_QM = true;
				in_TEXT = true;
				if (in_SPACE)
				{
					argv[argc] = _argv + j;
					++argc;
				}
				in_SPACE = false;
				break;
			case ' ':
			case '\t':
			case '\n':
			case '\r':
				if (in_TEXT)
				{
					_argv[j] = '\0';
					++j;
				}
				in_TEXT = false;
				in_SPACE = true;
				break;
			default:
				in_TEXT = true;
				if (in_SPACE)
				{
					argv[argc] = _argv + j;
					++argc;
				}
				_argv[j] = a;
				++j;
				in_SPACE = false;
				break;
			}
		}
		++i;
	}
	_argv[j] = '\0';
	argv[argc] = nullptr;

	(*_argc) = argc;
	return argv;
}

#define ToTString std::to_string
#endif


class WBufferRef
{
public:
	WBufferRef(std::vector<unsigned char>& Vec)
		: Container(Vec)
	{}
	
public:
	template<typename T>
	void Emplace(const T& V)
	{
		const size_t OldLen = Container.size();
		Container.resize(OldLen + sizeof(V));
		memcpy_s(Container.data() + OldLen, sizeof(V), &V, sizeof(V));
	}

	template<typename T, typename J>
	void EmplaceVector(const std::vector<T, J>& Vec)
	{
		const size_t OldLen = Container.size();
		const unsigned long long VecLen = Vec.size() * sizeof(T);
		Container.resize(OldLen + sizeof(VecLen) + VecLen);
		memcpy_s(Container.data() + OldLen, sizeof(VecLen), &VecLen, sizeof(VecLen));
		memcpy_s(Container.data() + OldLen + sizeof(VecLen), VecLen, Vec.data(), VecLen);
	}

	unsigned char* EmplaceRaw(size_t Size)
	{
		const size_t OldLen = Container.size();
		Container.resize(OldLen + Size);
		return Container.data() + OldLen;
	}


public:
	const unsigned char* Get() const
	{
		return Container.data();
	}
	size_t Num() const
	{
		return Container.size();
	}

	
private:
	std::vector<unsigned char>& Container;
};
class WBuffer : public WBufferRef
{
public:
	WBuffer()
		: WBufferRef(Actual)
	{}
	
private:
	std::vector<unsigned char> Actual;
};

class RBufferRef
{
public:
	RBufferRef(const std::vector<unsigned char>& Vec)
		: Container(Vec)
		, Pointer(0u)
	{}
	
public:
	bool IsEmpty() const
	{
		return Container.empty();
	}

public:
	bool IsReadable() const
	{
		return (Pointer < Container.size());
	}
	
	template<typename T>
	T Read()
	{
		T Ret;
		memcpy_s(&Ret, sizeof(T), Container.data() + Pointer, sizeof(T));
		Pointer += sizeof(T);
		return std::move(Ret);
	}

	template<typename T, typename J>
	void ReadVector(std::vector<T, J>* Vec)
	{
		unsigned long long Len;
		memcpy_s(&Len, sizeof(Len), Container.data() + Pointer, sizeof(Len));
		Pointer += sizeof(Len);

		Vec->resize(Len / sizeof(T));
		memcpy_s(Vec->data(), Len, Container.data() + Pointer, Len);
		Pointer += Len;
	}

	const unsigned char* ReadRaw(size_t Len)
	{
		const unsigned char* Ret = Container.data() + Pointer;
		Pointer += Len;
		return Ret;
	}
	
private:
	const std::vector<unsigned char>& Container;
protected:
	size_t Pointer;
};
class RBuffer : public RBufferRef
{
public:
	RBuffer()
		: RBufferRef(Actual)
	{}

public:
	void Resize(size_t Len)
	{
		Actual.resize(Len);
		Pointer = 0u;
	}
	unsigned char* Buffer()
	{
		return Actual.data();
	}

private:
	std::vector<unsigned char> Actual;
};


long long EarnCurrentTime()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}


template<typename OPEN, typename CLOSE, typename FUNC>
unsigned char CallFunc(OPEN&& Open, CLOSE&& Close, FUNC&& Func)
{
	if (!Open())
	{
		return 0xff;
	}

	unsigned char Flag = Func() ? 0x00 : 0x01;

	if (!Close())
	{
		Flag |= 0x02;
	}

	return Flag;
}


template<typename FUNC>
unsigned char ClipboardFunc(FUNC&& Func)
{
	return CallFunc([](){ return OpenClipboard(nullptr) == TRUE; }, [](){ return CloseClipboard() == TRUE; }, std::forward<FUNC>(Func));
}


bool CheckIfPredefinedFormat(UINT Format)
{
	switch (Format)
	{
	case CF_TEXT:
	case CF_BITMAP:
	case CF_METAFILEPICT:
	case CF_SYLK:
	case CF_DIF:
	case CF_TIFF:
	case CF_OEMTEXT:
	case CF_DIB:
	case CF_PALETTE:
	case CF_PENDATA:
	case CF_RIFF:
	case CF_WAVE:
	case CF_UNICODETEXT:
	case CF_ENHMETAFILE:
	case CF_HDROP:
	case CF_LOCALE:
	case CF_DIBV5:
		break;
	default:
		return false;
	}
	return true;
}

template<typename T>
bool CopyTextData(HANDLE Handle, std::vector<unsigned char>* Buffer)
{
	const T* Data = reinterpret_cast<const T*>(GlobalLock(Handle));
	if (!Data)
	{
		return false;
	}

	size_t Len = 0u;
	for (const T* Cur = Data; (*Cur) != 0; ++Cur, ++Len);
	Buffer->resize((Len + 1u) * sizeof(T));
	memcpy_s(Buffer->data(), Len * sizeof(T), Data, Len * sizeof(T));
	reinterpret_cast<T*>(Buffer->data())[Len] = 0;

	if (GlobalUnlock(Handle) != S_OK)
	{
		return false;
	}

	return true;
}
HGLOBAL PasteTextData(const std::vector<unsigned char>& Buffer)
{
	HGLOBAL Assigned = GlobalAlloc(GMEM_MOVEABLE, Buffer.size());
	if (!Assigned)
	{
		return nullptr;
	}

	void* Raw = GlobalLock(Assigned);
	if (!Raw)
	{
		GlobalFree(Assigned);
		return nullptr;
	}

	memcpy_s(Raw, Buffer.size(), Buffer.data(), Buffer.size());

	if (GlobalUnlock(Assigned) != S_OK)
	{
		GlobalFree(Assigned);
		return nullptr;
	}

	return Assigned;
}

bool CopyBitmapData(HANDLE Handle, std::vector<unsigned char>* Buffer)
{
	HDC Hdc = GetDC(nullptr);
	if (!Hdc)
	{
		return false;
	}

	HBITMAP HandleAsBitmap = static_cast<HBITMAP>(Handle);

	BITMAPINFO BitmapInfo = { 0 };
	BitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	if (!GetDIBits(Hdc, HandleAsBitmap, 0u, 0u, nullptr, &BitmapInfo, DIB_RGB_COLORS))
	{
		ReleaseDC(nullptr, Hdc);
		return false;
	}
	//BitmapInfo.bmiHeader.biCompression = BI_RGB;

	WBufferRef Writer(*Buffer);

	Writer.Emplace(BitmapInfo);

	unsigned char* Pixels = Writer.EmplaceRaw(BitmapInfo.bmiHeader.biSizeImage);
	if (!GetDIBits(Hdc, HandleAsBitmap, 0u, BitmapInfo.bmiHeader.biHeight, Pixels, &BitmapInfo, DIB_RGB_COLORS))
	{
		ReleaseDC(nullptr, Hdc);
		return false;
	}

	ReleaseDC(nullptr, Hdc);
	return true;
}
HGLOBAL PasteImageData(UINT* Format, const std::vector<unsigned char>& Buffer)
{
	RBufferRef Reader(Buffer);
	
	BITMAPINFO BitmapInfo = Reader.Read<BITMAPINFO>();

	HGLOBAL Assigned = GlobalAlloc(GMEM_MOVEABLE | GMEM_SHARE, sizeof(BITMAPINFOHEADER) + BitmapInfo.bmiHeader.biSizeImage);
	if (!Assigned)
	{
		return nullptr;
	}

	void* Raw = GlobalLock(Assigned);
	if (!Raw)
	{
		GlobalFree(Assigned);
		return nullptr;
	}

	memcpy_s(Raw, sizeof(BITMAPINFOHEADER), &BitmapInfo.bmiHeader, sizeof(BITMAPINFOHEADER));
	memcpy_s(Raw, BitmapInfo.bmiHeader.biSizeImage, Reader.ReadRaw(BitmapInfo.bmiHeader.biSizeImage), BitmapInfo.bmiHeader.biSizeImage);

	if (GlobalUnlock(Assigned) != S_OK)
	{
		GlobalFree(Assigned);
		return nullptr;
	}

	(*Format) = CF_DIB;
	return Assigned;
}

bool CopyDataFromHandle(HANDLE Handle, UINT Format, std::vector<unsigned char>* Buffer)
{
	switch (Format)
	{
	case CF_TEXT:
	case CF_OEMTEXT:
		if (CopyTextData<char>(Handle, Buffer))
		{
			return false;
		}
		break;
	case CF_UNICODETEXT:
		if (CopyTextData<wchar_t>(Handle, Buffer))
		{
			return false;
		}
		break;
	case CF_BITMAP:
		if (!CopyBitmapData(Handle, Buffer))
		{
			return false;
		}
		break;
	case CF_METAFILEPICT:
	case CF_SYLK:
	case CF_DIF:
	case CF_TIFF:
	case CF_DIB:
	case CF_PALETTE:
	case CF_PENDATA:
	case CF_RIFF:
	case CF_WAVE:
	case CF_ENHMETAFILE:
	case CF_HDROP:
	case CF_LOCALE:
	case CF_DIBV5:
		break;
	default:
		return false;
	}
	return true;
}
HGLOBAL PasteDataAsHandle(UINT* Format, const std::vector<unsigned char>& Buffer)
{
	HGLOBAL Assigned = nullptr;
	switch (*Format)
	{
	case CF_TEXT:
	case CF_OEMTEXT:
	case CF_UNICODETEXT:
		Assigned = PasteTextData(Buffer);
		break;
	case CF_BITMAP:
		Assigned = PasteImageData(Format, Buffer);
		break;
	case CF_METAFILEPICT:
	case CF_SYLK:
	case CF_DIF:
	case CF_TIFF:
	case CF_DIB:
	case CF_PALETTE:
	case CF_PENDATA:
	case CF_RIFF:
	case CF_WAVE:
	case CF_ENHMETAFILE:
	case CF_HDROP:
	case CF_LOCALE:
	case CF_DIBV5:
		break;
	default:
		return nullptr;
	}
	return Assigned;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


bool WriteToFile(const std::filesystem::path& FilePath)
{
	static robin_hood::unordered_set<UINT> Formats;
	static WBuffer Container;

	if (ClipboardFunc([]()
	{
		{
			Formats.clear();
			UINT Format = 0u;
			while ((Format = EnumClipboardFormats(Format)) != 0u)
			{
				if (!CheckIfPredefinedFormat(Format))
				{
					continue;
				}
				Formats.emplace(Format);
			}
		}

		for (UINT Format : Formats)
		{
			HANDLE ClipboardHandle = GetClipboardData(Format);
			if (!ClipboardHandle)
			{
				continue;
			}

			std::vector<unsigned char> Buffer;
			if (!CopyDataFromHandle(ClipboardHandle, Format, &Buffer))
			{
				continue;
			}
			if (Buffer.empty())
			{
				continue;
			}

			Container.Emplace(Format);
			Container.EmplaceVector(Buffer);
		}

		return true;
	}) == 0xff)
	{
		return true;
	}

	for (const auto& OldFile : std::filesystem::directory_iterator(FilePath.parent_path()))
	{
		std::filesystem::remove(OldFile);
	}

	bool bWriteError = false;
	do{
		std::unique_ptr<FILE, void(*)(FILE*)> File(nullptr, [](FILE* P){ fclose(P); });
		{
			FILE* P;
			_tfopen_s(&P, FilePath.c_str(), TEXT("wb"));
			if (!P)
			{
				break;
			}
			File.reset(P);
		}
		
		const unsigned long long Len = Container.Num();
		if (fwrite(&Len, sizeof(Len), 1u, File.get()) != 1u)
		{
			bWriteError = true;
			break;
		}
		if (fwrite(Container.Get(), 1u, Len, File.get()) != Len)
		{
			bWriteError = true;
			break;
		}
	}
	while (false);
	if (bWriteError)
	{
		std::filesystem::remove(FilePath);
	}

	return true;
}

unsigned char ReadOnFile(const std::filesystem::path& FilePath)
{
	static RBuffer Container;
	
	{
		std::unique_ptr<FILE, void(*)(FILE*)> File(nullptr, [](FILE* P){ fclose(P); });
		{
			FILE* P;
			_tfopen_s(&P, FilePath.c_str(), TEXT("rb"));
			if (!P)
			{
				return 0x00;
			}
			File.reset(P);
		}

		unsigned long long Len = 0u;
		if (fread_s(&Len, sizeof(Len), sizeof(Len), 1u, File.get()) != 1u)
		{
			return 0x00;
		}
		if (Len > 0u)
		{
			Container.Resize(Len);
			if (fread_s(Container.Buffer(), Len, 1u, Len, File.get()) != Len)
			{
				return 0x00;
			}
		}
		else
		{
			Container.Resize(0u);
		}
	}

	if (Container.IsEmpty())
	{
		return 0x01;
	}

	if (ClipboardFunc([]()
	{
		if (!EmptyClipboard())
		{
			return false;
		}

		while (Container.IsReadable())
		{
			UINT Format = Container.Read<decltype(Format)>();
			std::vector<unsigned char> Data;
			Container.ReadVector(&Data);

			HGLOBAL Assigned = PasteDataAsHandle(&Format, Data);
			if (!Assigned)
			{
				continue;
			}
			
			if (!SetClipboardData(Format, Assigned))
			{
				GlobalFree(Assigned);
				continue;
			}
		}

		return true;
	}) & 0x01)
	{
		return 0x00;
	}

	return 0x01;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


std::filesystem::path WindowsMainFilePath;


LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool bListening = false;
	switch (uMsg)
	{
	case WM_CREATE:
		bListening = (AddClipboardFormatListener(hWnd) == TRUE);
		return bListening ? 0 : -1;

	case WM_DESTROY:
		if (bListening)
		{
			RemoveClipboardFormatListener(hWnd);
			bListening = false;
		}
		return 0;

	case WM_CLIPBOARDUPDATE:
		if (!WriteToFile(WindowsMainFilePath / ToTString(EarnCurrentTime())))
		{
			PostQuitMessage(-1);
		}
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
int _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nShowCmd)
{
	bool bAsRead = false;
	{
		size_t Argc;
		std::unique_ptr<PTCHAR, void(*)(PTCHAR*)> Argv(CommandLineToArgvT(lpCmdLine, &Argc), [](PTCHAR* P){ GlobalFree(P); });
	
		if (Argc >= 2u)
		{
			int ID = tolower(*Argv.get()[1]);
			switch (ID)
			{
			case 'r':
				bAsRead = true;
				break;
	
			case 'w':
				bAsRead = false;
				break;
	
			default:
				return -1;
			}
		
			WindowsMainFilePath = Argv.get()[0];
		}
		else
		{
			WindowsMainFilePath = TEXT("X:\\clipboard");
			bAsRead = true;
		}
	}
	
	if (!bAsRead)
	{
		WNDCLASSEX WndClass = { sizeof(WNDCLASSEX) };
		{
			WndClass.hInstance = hInstance;
			WndClass.lpfnWndProc = WndProc;
			WndClass.lpszClassName = TEXT("ClipboardDisk");
		}
		if (!RegisterClassEx(&WndClass))
		{
			return -1;
		}

		HWND HWnd = CreateWindowEx(0, WndClass.lpszClassName, TEXT(""), 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandle(nullptr), nullptr);
		if (!HWnd)
		{
			return -1;
		}

		MSG Message;
		while (GetMessage(&Message, nullptr, 0, 0))
		{
			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}
	}
	else
	{
		long long LastReadIndex = MINLONGLONG;
		std::filesystem::path TmpFilePath;
		
		long long MaxIndex = MINLONGLONG;
		for (;; Sleep(WAIT_TIME))
		{
			bool bNoFile = true;
			for (auto& CurDir : std::filesystem::directory_iterator(WindowsMainFilePath))
			{
				if (CurDir.is_directory())
				{
					continue;
				}

#ifdef UNICODE
				long long CurIndex = std::stoll(CurDir.path().stem().wstring());
#else
				long long CurIndex = std::stoll(CurDir.path().stem().string());
#endif

				if (MaxIndex < CurIndex)
					MaxIndex = CurIndex;

				bNoFile = false;
			}
			if (bNoFile)
			{
				continue;
			}

			if (LastReadIndex == MaxIndex)
			{
				continue;
			}
			
			TmpFilePath = WindowsMainFilePath / ToTString(MaxIndex);
			if (!std::filesystem::exists(TmpFilePath))
			{
				continue;
			}

			unsigned char bRes = ReadOnFile(TmpFilePath);
			if (bRes == 0xff)
			{
				return -1;
			}
			else if (bRes == 0x00)
			{
				continue;
			}

			LastReadIndex = MaxIndex;
		}
	}

	return 0;
}


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


#undef WAIT_TIME


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

