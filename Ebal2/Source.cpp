#include <Windows.h>
#include <windowsx.h>
#include <d2d1.h>
#include <list>
#include <memory>
#pragma comment(lib, "d2d1")
#include "basewin.h"
#include "resource1.h"
#include "resource2.h"
using namespace std;

int numColor = 0;
HINSTANCE abc;

template <class T>
void SafeRelease(T** ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

class DPIScale
{
	static float scaleX;
	static float scaleY;
public:
	static void Initialize(ID2D1Factory* pFactory)
	{
		FLOAT dpiX, dpiY;
		pFactory->GetDesktopDpi(&dpiX, &dpiY);
		scaleX = dpiX / 96.0f;
		scaleY = dpiY / 96.0f;
	}

	template <typename T>
	static float PixelsToDipsX(T x)
	{
		return static_cast<float>(x) / scaleX;
	}

	template <typename T>
	static float PixelsToDipsY(T y)
	{
		return static_cast<float>(y) / scaleY;
	}

	template <typename T>
	static D2D1_POINT_2F PixelsToDips(T x, T y)
	{
		return D2D1::Point2F(static_cast<float>(x) / scaleX, static_cast<float>(y) / scaleY);
	}
};

float DPIScale::scaleX = 1.0f;
float DPIScale::scaleY = 1.0f;

struct MyEllipse
{
	D2D1_ELLIPSE ellipse;
	D2D1_COLOR_F color;

	void Draw(ID2D1RenderTarget* pRT, ID2D1SolidColorBrush* pBrush, BOOL isSelect)
	{
		pBrush->SetColor(color);
		pRT->FillEllipse(ellipse, pBrush);
		pBrush->SetColor(isSelect ? D2D1::ColorF(D2D1::ColorF::White) : D2D1::ColorF(D2D1::ColorF::Black));
		pRT->DrawEllipse(ellipse, pBrush, 1.0f);
	}

	BOOL HitTest(float x, float y)
	{
		const float a = ellipse.radiusX;
		const float b = ellipse.radiusY;
		const float x1 = x - ellipse.point.x;
		const float y1 = y - ellipse.point.y;
		const float d = ((x1 * x1) / (a * a)) + ((y1 * y1) / (b * b));
		return d <= 1.0f;
	}
};

class MainWindow : public BaseWindow<MainWindow>
{
	HMENU hMenubar;


	ID2D1Factory* pFactory;
	ID2D1HwndRenderTarget* pRenderTarget;
	ID2D1SolidColorBrush* pBrush;
	D2D1_ELLIPSE ellipse;
	D2D1_POINT_2F ptMouse;
	HCURSOR hCursor;
	list<shared_ptr<MyEllipse>> ellipses;
	list<shared_ptr<MyEllipse>>::iterator selection;

	enum Mode { DrawMode, SelectMode, DragMode } mode;


	static D2D1::ColorF colors[7];

	void CalculateLayout()
	{
	}

	HRESULT CreateGraphicsResources();
	void DiscardGraphicsResources();
	void Resize();

	shared_ptr<MyEllipse> Selection()
	{
		if (selection == ellipses.end())
		{
			return nullptr;
		}
		else
		{
			return (*selection);
		}
	}

	void ClearSelection() { selection = ellipses.end(); }

	void SetMode(Mode m)
	{
		mode = m;
		// Update the cursor
		LPWSTR cursor = IDC_ARROW;
		switch (mode)
		{
		case DrawMode:
			cursor = IDC_CROSS;
			break;
		case SelectMode:
			cursor = IDC_HAND;
			break;
		case DragMode:
			cursor = IDC_SIZEALL;
			break;
		}
		hCursor = LoadCursor(NULL, cursor);
		SetCursor(hCursor);
	}

	void InsertEllipse(float x, float y);
	void DeleteEllipse();
	BOOL HitTest(float x, float y);
	void OnPaintScene();
public:
	MainWindow() : pFactory(NULL), pRenderTarget(NULL),
	               ptMouse(D2D1::Point2F())

	{
		ClearSelection();
		SetMode(DrawMode);
	}

	PCWSTR ClassName() const { return L"Ellipses Window Class"; }
	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
	void OnLButtonDown(int pixelX, int pixelY, DWORD flags);
	void OnLButtonUp();
	void OnMouseMove(int pixelX, int pixelY, DWORD flags);
	void OnPaint();
};

D2D1::ColorF MainWindow::colors[7] = {
	D2D1::ColorF::Red, D2D1::ColorF::Orange, D2D1::ColorF::Yellow,
	D2D1::ColorF::Green, D2D1::ColorF::Cyan, D2D1::ColorF::Blue, D2D1::ColorF::Violet
};

void MainWindow::InsertEllipse(float x, float y)
{
	shared_ptr<MyEllipse> pme(new MyEllipse);
	D2D1_POINT_2F pt = {x, y};
	pme->ellipse.point = ptMouse = pt;
	pme->ellipse.radiusX = pme->ellipse.radiusY = 1.0f;
	pme->color = colors[numColor];
	ellipses.push_front(pme);
	selection = ellipses.begin();
}

void MainWindow::DeleteEllipse()
{
	ellipses.erase(selection);
	ClearSelection();
}

BOOL MainWindow::HitTest(float x, float y)
{
	list<shared_ptr<MyEllipse>>::iterator iter;
	for (iter = ellipses.begin(); iter != ellipses.end(); iter++)
		if ((*iter)->HitTest(x, y))
		{
			selection = iter;
			return TRUE;
		}
	return 0;
}

HRESULT MainWindow::CreateGraphicsResources()
{
	HRESULT hr = S_OK;
	if (pRenderTarget == NULL)
	{
		RECT rc;
		GetClientRect(m_hwnd, &rc);
		D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);
		hr = pFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(m_hwnd, size),
			&pRenderTarget);
		if (SUCCEEDED(hr))
		{
			const D2D1_COLOR_F color = D2D1::ColorF(1.0f, 1.0f, 0);
			hr = pRenderTarget->CreateSolidColorBrush(color, &pBrush);
			if (SUCCEEDED(hr))
			{
				CalculateLayout();
			}
		}
	}
	return hr;
}

void MainWindow::DiscardGraphicsResources()
{
	SafeRelease(&pRenderTarget);
	SafeRelease(&pBrush);
}

void MainWindow::OnPaintScene()
{
	list<shared_ptr<MyEllipse>>::reverse_iterator riter;
	for (riter = ellipses.rbegin(); riter != ellipses.rend(); riter++)
		(*riter)->Draw(pRenderTarget, pBrush, (Selection() && (*riter == *selection)));
}

void MainWindow::OnLButtonDown(int pixelX, int pixelY, DWORD flags)
{
	const float dipX = DPIScale::PixelsToDipsX(pixelX);
	const float dipY = DPIScale::PixelsToDipsY(pixelY);
	if (mode == DrawMode)
	{
		POINT pt = {pixelX, pixelY};
		if (DragDetect(m_hwnd, pt))
		{
			SetCapture(m_hwnd);
			// Start a new ellipse.
			InsertEllipse(dipX, dipY);
		}
	}
	else
	{
		ClearSelection();
		if (HitTest(dipX, dipY))
		{
			SetCapture(m_hwnd);
			ptMouse = Selection()->ellipse.point;
			ptMouse.x -= dipX;
			ptMouse.y -= dipY;
			SetMode(DragMode);
		}
	}
	InvalidateRect(m_hwnd, NULL, FALSE);
}

void MainWindow::OnMouseMove(int pixelX, int pixelY, DWORD flags)
{
	const float dipX = DPIScale::PixelsToDipsX(pixelX);
	const float dipY = DPIScale::PixelsToDipsY(pixelY);
	if ((flags & MK_LBUTTON) && Selection())
	{
		if (mode == DrawMode)
		{
			// Resize the ellipse.
			const float width = (dipX - ptMouse.x) / 2;
			const float height = (dipY - ptMouse.y) / 2;
			const float x1 = ptMouse.x + width;
			const float y1 = ptMouse.y + height;
			Selection()->ellipse = D2D1::Ellipse(D2D1::Point2F(x1, y1), width, height);
		}
		else if (mode == DragMode)
		{
			// Move the ellipse.
			Selection()->ellipse.point.x = dipX + ptMouse.x;
			Selection()->ellipse.point.y = dipY + ptMouse.y;
		}
		InvalidateRect(m_hwnd, NULL, FALSE);
	}
}

void MainWindow::OnLButtonUp()
{
	if ((mode == DrawMode) && Selection())
	{
		ClearSelection();
		InvalidateRect(m_hwnd, NULL, FALSE);
	}
	else if (mode == DragMode)
	{
		SetMode(SelectMode);
	}
	ReleaseCapture();
}

void MainWindow::OnPaint()
{
	HRESULT hr = CreateGraphicsResources();
	if (SUCCEEDED(hr))
	{
		PAINTSTRUCT ps;
		BeginPaint(m_hwnd, &ps);
		pRenderTarget->BeginDraw();
		pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));
		OnPaintScene();
		hr = pRenderTarget->EndDraw();
		if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
		{
			DiscardGraphicsResources();
		}
		EndPaint(m_hwnd, &ps);
	}
}

void MainWindow::Resize()
{
	if (pRenderTarget != NULL)
	{
		RECT rc;
		GetClientRect(m_hwnd, &rc);
		D2D1_SIZE_U size = D2D1::SizeU(rc.right, rc.bottom);
		pRenderTarget->Resize(size);
		CalculateLayout();
		InvalidateRect(m_hwnd, NULL, FALSE);
	}
}

BOOL CALLBACK DlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG: // �������������
		SetWindowPos(hwnd,HWND_TOP, 5, 300, 0, 0, SWP_NOSIZE);
		break;
	case WM_COMMAND: // ���������� ������ ������
		switch (LOWORD(wParam))
		{
		case IDC_BUTTON1:

			numColor = 0;
			EndDialog(hwnd, 0);
			return FALSE;

		case IDC_BUTTON2:

			numColor = 1;
			EndDialog(hwnd, 0);
			return FALSE;

		case IDC_BUTTON3:

			numColor = 2;
			EndDialog(hwnd, 0);
			return FALSE;

		case IDC_BUTTON4:

			numColor = 3;
			EndDialog(hwnd, 0);
			return FALSE;

		case IDC_BUTTON5:

			numColor = 4;
			EndDialog(hwnd, 0);
			return FALSE;

		case IDC_BUTTON6:

			numColor = 5;
			EndDialog(hwnd, 0);
			return FALSE;

		case IDC_BUTTON7:

			numColor = 6;
			EndDialog(hwnd, 0);
			return FALSE;
		}


		break;
	case WM_CLOSE:
		EndDialog(hwnd, 0);
		return FALSE;
	}
	return FALSE;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
	MainWindow win;
	HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCEL1));
	if (!win.Create(L"Ellipses", WS_OVERLAPPEDWINDOW))
	{
		return 0;
	}
	ShowWindow(win.Window(), nCmdShow);
	abc = hInstance;
	// ���� ��������� ���������.
	MSG msg = {};
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(win.Window(), hAccel, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return 0;
}

LRESULT MainWindow::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
		if (FAILED(D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory)))
		{
			return -1; // ������� CreateWindowEx.
		}
		DPIScale::Initialize(pFactory);
		hMenubar = CreateMenu();
		AppendMenu(hMenubar, MF_ENABLED | MF_STRING, 123, TEXT("����"));
		SetMenu(m_hwnd, hMenubar);
		return 0;
	case WM_DESTROY:
		DiscardGraphicsResources();
		SafeRelease(&pFactory);
		PostQuitMessage(0);
		return 0;
	case WM_PAINT:
		OnPaint();
		return 0;
	case WM_SIZE:
		Resize();
		return 0;
	case WM_LBUTTONDOWN:
		OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (DWORD)wParam);
		return 0;
	case WM_LBUTTONUP:
		OnLButtonUp();
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), (DWORD)wParam);
		return 0;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case 123:
			DialogBoxParam(abc, MAKEINTRESOURCE(IDD_DIALOG1), GetActiveWindow(), (DlgProc), 0);
			break;
		case ID_DRAW_MODE:
			SetMode(DrawMode);
			break;
		case ID_SELECT_MODE:
			SetMode(SelectMode);
			break;
		case ID_TOGGLE_MODE:
			if (mode == DrawMode)
			{
				SetMode(SelectMode);
			}
			else
			{
				SetMode(DrawMode);
			}
			break;
		case ID_DELETE:
			if (Selection())
			{
				DeleteEllipse();
				InvalidateRect(m_hwnd, NULL, FALSE);
			}
			break;
		}
		return 0;
	case WM_SETCURSOR:
		if (LOWORD(lParam) == HTCLIENT)
		{
			SetCursor(hCursor);
			return TRUE;
		}
		return 0;
	}
	return DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}
