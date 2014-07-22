#include <windows.h>
#include <stdio.h>
#include <malloc.h>
#include <math.h>
#include "counters.h"

typedef unsigned long uint32;
typedef long int32;

// ================================================================================================
// Makes UI and bitmaps and stuff
// ================================================================================================
class NetMeter {

	// minimal UI bits
	HWND hwnd;
	HBITMAP hBitmap;
	HFONT font;
	
	long width, height;
	unsigned long *rawBits;

	// everything else
	PDH psend;
	PDH precv;

	ATOM Register(HINSTANCE hinst) {

		WNDCLASS wc;

		// register the main window class
		wc.style = 0;
		wc.lpfnWndProc = (WNDPROC)wproc;
		wc.cbClsExtra = 0;
		wc.cbWndExtra = 0;
		wc.hInstance = hinst;
		wc.hIcon = LoadIcon((HINSTANCE) NULL, IDI_APPLICATION);
		wc.hCursor = LoadCursor((HINSTANCE) NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
		wc.lpszMenuName =  NULL;
		wc.lpszClassName = "NM";

		return RegisterClass(&wc);
	}

	static void CALLBACK tproc(HWND hwnd, UINT uMsg, UINT_PTR idEvent, DWORD dwTime) 
	{
		NetMeter *nm = (NetMeter*)idEvent;
		nm->Timer();
	}

	int Initialize(int w, int h) {

		//SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
		width = w;
		height = h;

		HINSTANCE hi = GetModuleHandle(NULL);
		ATOM netclass = Register(hi);

		hwnd = ::CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "NM", "", WS_POPUP,
		 						256, 16, width, height,
								(HWND)NULL, (HMENU)NULL, hi, LPVOID(NULL));

		int err  =GetLastError();

		if (!hwnd) return -1;

		::SetWindowLong(hwnd, GWL_USERDATA, (long)this);
		::SetWindowLong(hwnd, GWL_WNDPROC, (long)wproc);

		{
			BITMAPINFO bmi;
			int bmihsize = sizeof(BITMAPINFOHEADER);
			memset(&bmi, 0, bmihsize);

			BITMAPINFOHEADER &h = bmi.bmiHeader;

			h.biSize		= bmihsize;
			h.biWidth		= width;
			h.biHeight		= -height;
			h.biPlanes		= 1;
			h.biBitCount	= 32;
			h.biCompression	= BI_RGB;

			hBitmap = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, (void**)&rawBits, NULL, 0);

			if (!hBitmap) return -1;
		}

		{
			// make font
			LOGFONT lf;
			memset(&lf, 0, sizeof(LOGFONT));

			strcpy(lf.lfFaceName, "Arial");
			lf.lfHeight = 14;
			font = ::CreateFontIndirect(&lf);
		}

		::ShowWindow((HWND)hwnd, SW_SHOWNORMAL);
		::SetTimer(hwnd, (long)this, 1000, tproc);

		return 0;
	}

	void Smooth(long *v, int32 ct) {

		int32 i;
		const float soft = 0.1f;
		const float isoft = 1.f - soft;
		float weight = 0;
		float value  = 0;

		for (i = 0; i < ct; i++) {
			weight = weight * isoft + soft;
			value  = value  * isoft + soft * v[i];

			v[i] = value / weight;
		}

		weight = value = 0;

		for (i = ct - 1; i >= 0; i--) {
			weight = weight * isoft + soft;
			value  = value  * isoft + soft * v[i];

			v[i] = value / weight;
		}
	}

	int Redraw() {

		int32 i, x, y;

		const uint32 ocol = 0x45433C;
		const uint32 dcol = 0xD6CFBB;

		const uint32 upcol = 0xF90505;
		const uint32 dncol = 0x19469E;

		if (!rawBits) return -1;

		int32 clipheight = height - 20;

		// plot history from pdh
		for (i = 0; i < width * height; i++)  {
			rawBits[i] = 0xEEE7D0;
		}

		long *vs = (long*)alloca(width * 4);
		long *vr = (long*)alloca(width * 4);

		long maxval = 0;
		long maxr = 0;
		long maxs = 0;

		for (i = 0; i < width; i++) {
			vs[i] = psend.Sum(i);
			vr[i] = precv.Sum(i);

			maxs = max(vs[i], maxs);
			maxr = max(vr[i], maxr);

			maxval = max(vs[i], maxval);
			maxval = max(vr[i], maxval);
		}

		long midmax = min(maxs, maxr);
		{
			int32 v1 = midmax * clipheight / maxval;
			v1 = clipheight - v1 - 1;
			for (x = 1; x < width - 1; x++) {
				rawBits[x + width * v1] = dcol;
			}
		}

		//for (int smooth = 0; smooth < 2; smooth ++) {
			for (x = 0; x < width; x++) {

				int32 i = (width - x - 1);
				
				int32 sc1 = vs[i] * clipheight / maxval;
				int32 sc2 = vr[i] * clipheight / maxval;

				sc1 = clipheight - sc1 - 1;
				sc2 = clipheight - sc2 - 1;

				/*
				if (smooth) {
					if (vs[i] || vr[i]) {
						uint32 *pixel1 = rawBits + x + width * sc1;
						uint32 *pixel2 = rawBits + x + width * sc2;

						if (sc1 < sc2) {
							pixel1[0] = 0;
						} else {
							pixel2[0] = 0;
						}
					}
				} */
				for (int32 y = (clipheight - 1); y >= 0; y--) {

					uint32 *pixel = rawBits + x + y * width;
					if (y < sc1 && y < sc2) break;

					/*if (smooth) {
						if (y > sc2 || y > sc1) pixel[0] = (pixel[0] >> 1 & 0x7F7F7F) + 0x7f7f7f;
					} else {*/
						if (y > sc2) pixel[0] = dncol;
						if (y > sc1) pixel[0] = upcol;
					//}
				}
			}
			//Smooth(vs, width);
			//Smooth(vr, width);
		//}


		// outline thing

		for (y = 0; y < height; y++) {
			rawBits[0 + y * width] = ocol;
			rawBits[(y + 1) * width - 1] = ocol;
		}
		for (x = 1; x < width - 1; x++) {
			rawBits[x] = ocol;
			rawBits[x + width * (height - 1)] = ocol;
			rawBits[x + width * (clipheight)] = dcol;
		}

		{
			HDC hdc = (HDC)CreateCompatibleDC(NULL);
			::SelectObject(hdc, hBitmap);
			
			::SetTextAlign(hdc, TA_BOTTOM | TA_CENTER);
			::SetBkMode(hdc, TRANSPARENT);

			::SelectObject(hdc, font);

			char temp[200];
			sprintf(temp, "Down: %dKB/s   Up: %dKB/s   Max: %dKB/s", vr[0] / 1024, vs[0] / 1024, maxval / 1024);
			TextOut(hdc, width / 2, height - 4, temp, strlen(temp));
			::DeleteDC(hdc);
		}

		return Draw();
	}

	int Draw(HDC hdc = NULL) {

		HDC hcdc = CreateCompatibleDC(NULL);
		HDC dc = hdc;

		if (!dc) dc = (HDC)GetDC(hwnd);
		HBITMAP oldbitmap = (HBITMAP)SelectObject((HDC)hcdc, hBitmap);
		
		::BitBlt(dc, 0, 0, width, height, (HDC)hcdc, 0, 0, SRCCOPY);

		int e = GetLastError();
		
		SelectObject((HDC)hcdc, oldbitmap);
		DeleteDC(hcdc);
		DeleteDC(dc);
		::GdiFlush();

		return 0;
	}

	static int __stdcall wproc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
		NetMeter *nm = (NetMeter*)::GetWindowLong(hwnd, GWL_USERDATA);

		long rv = nm->WindowProc(uMsg, wParam, lParam);
		if (rv != -1) return rv;

		// -1 => default
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

public:
	NetMeter() : rawBits(NULL), hwnd(NULL), hBitmap(NULL) {
		psend.AddObjects("Network Interface", "Bytes Sent");
		precv.AddObjects("Network Interface", "Bytes Received");
		Initialize(256, 96);
	}

	int Timer() {
		psend.Tick();
		precv.Tick();
		return Redraw();
	}

	int WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam) {

		if (uMsg == WM_LBUTTONDOWN) {
			// drag hack
			::SendMessage(hwnd, WM_SYSCOMMAND, SC_MOVE | 0x02, 0);
		}

		if (uMsg == WM_TIMER) {
			Timer();
		}
		if (uMsg == WM_ERASEBKGND) {
			return 0;
		}
		if (uMsg == WM_PAINT) {
			// stupid paint
			PAINTSTRUCT ps;

			HDC hdc = ::BeginPaint(hwnd, &ps);

			if(hdc != NULL)	{
				Draw(hdc);
				::EndPaint(hwnd, &ps);
			}
		}
		if (uMsg == WM_CLOSE) {
			::PostQuitMessage(0);
		}

		return -1;
	}
};

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	NetMeter nm;
	MSG msg;

	while (::GetMessage(&msg, (HWND)NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}
