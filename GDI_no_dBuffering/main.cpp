#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>


//#include

namespace offsets
{
	//CSGO
	std::uintptr_t m_bSpoted = 0x93D;
	std::uintptr_t entityList = 0x4E051DC;
	std::uintptr_t m_fFlashDuration = 0x2C44;
	std::uintptr_t m_iHealth = 0x100;
	std::uintptr_t m_fFalgs = 0x104;
	std::uintptr_t dwForceAttack = 0x52BCD88;
	std::uintptr_t dwViewMatrix = 0x4DF6024;
	std::uintptr_t m_bDormant = 0xED;
	std::uintptr_t dwLocalPlayer = 0xDEF97C;
	std::uintptr_t m_iTeamNum = 0xF4;
	std::uintptr_t m_vecOrigin = 0x138;
	std::ptrdiff_t m_vecViewOffset = 0x108;
}

#define EnemyPen 0x000000FF
HBRUSH EnemyBrush = CreateSolidBrush(0x000000FF);

int screenX = GetSystemMetrics(SM_CXSCREEN);
int screenY = GetSystemMetrics(SM_CYSCREEN);

DWORD GetProcId(const wchar_t* procName)
{
	uintptr_t procId = 0;
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE)
	{
		return 0;
	}

	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);
	if (Process32First(snap, &entry))
	{
		do {
			if (!_wcsicmp(entry.szExeFile, procName))
			{
				procId = entry.th32ProcessID;
				break;
			}
		} while (Process32Next(snap, &entry));
	}
	CloseHandle(snap);
	return procId;
}

uintptr_t GetModBase(DWORD procId, const wchar_t* modName)
{
	uintptr_t modBaseAdr = 0;
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, procId);
	if (snap == INVALID_HANDLE_VALUE)
	{
		return 0;
	}

	MODULEENTRY32 entry;
	entry.dwSize = sizeof(MODULEENTRY32);
	if (Module32First(snap, &entry))
	{
		do {
			if (!_wcsicmp(entry.szModule, modName))
			{
				modBaseAdr = (uintptr_t)entry.modBaseAddr;
				break;
			}
		} while (Module32Next(snap, &entry));
	}
	CloseHandle(snap);
	return modBaseAdr;
}

uintptr_t moduleBase = GetModBase(GetProcId(L"csgo.exe"), L"client.dll");
DWORD PID = GetProcId(L"csgo.exe");
HANDLE hprocess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetProcId(L"csgo.exe"));
HDC hdc = GetDC(FindWindowA(NULL, "Counter-Strike: Global Offensive"));

template <typename T> T RPM(SIZE_T address) {
	T buffer;

	ReadProcessMemory(hprocess, (LPCVOID)address, &buffer, sizeof(T), NULL);

	return buffer;
}

struct view_matrix_t
{
	float* operator[ ](int index) {
		return matrix[index];
	}
	float matrix[4][4];
};

struct Vec3
{
	float x, y, z;
};

Vec3 WorldToScreen(const Vec3 pos, view_matrix_t matrix)
{

	float _x = matrix[0][0] * pos.x + matrix[0][1] * pos.y + matrix[0][2] * pos.z + matrix[0][3];
	float _y = matrix[1][0] * pos.x + matrix[1][1] * pos.y + matrix[1][2] * pos.z + matrix[1][3];

	float w = matrix[3][0] * pos.x + matrix[3][1] * pos.y + matrix[3][2] * pos.z + matrix[3][3];

	float inv_w = 1.f / w;
	_x *= inv_w;
	_y *= inv_w;

	float x = screenX * .5f;
	float y = screenY * .5f;

	x += 0.5f * _x * screenX + 0.5f;
	y -= 0.5f * _y * screenY + 0.5f;

	return { x,y,w };
}

void DrawFilledRect(int x, int y, int w, int h) {
	RECT rect = { x,y,x + w, y + h };
	FillRect(hdc, &rect, EnemyBrush);
}

void DrawBorderBox(int x, int y, int w, int h, int thicknes)
{
	DrawFilledRect(x, y, w, thicknes); //Top horiz line
	DrawFilledRect(x, y, thicknes, h); //left vertic line
	DrawFilledRect((x + w), y, thicknes, h); //right vertic line
	DrawFilledRect(x, y + h, w + thicknes, thicknes); //bottom horiz line
}

void DrawLine(float StartX, float StartY, float EndX, float EndY)
{
	int a, b = 0;
	HPEN hOPen;
	HPEN hNPen = CreatePen(PS_SOLID, 2, EnemyPen);// penstyle, width, color
	hOPen = (HPEN)SelectObject(hdc, hNPen);
	MoveToEx(hdc, StartX, StartY, NULL); //start
	a = LineTo(hdc, EndX, EndY); //end
	DeleteObject(SelectObject(hdc, hOPen));
}


int main()
{


	while (true)
	{
		view_matrix_t vm = RPM<view_matrix_t>(moduleBase + offsets::dwViewMatrix);
		int localteam = RPM<int>(RPM<DWORD>(moduleBase + offsets::entityList) + offsets::m_iTeamNum);

		for (int i = 1; i < 64; i++)
		{
			uintptr_t pEnt = RPM<DWORD>(moduleBase + offsets::entityList + (i * 0x10));

			int health = RPM<int>(pEnt + offsets::m_iHealth);
			int team = RPM<int>(pEnt + offsets::m_iTeamNum);

			Vec3 pos = RPM<Vec3>(pEnt + offsets::m_vecOrigin);
			Vec3 head;
			head.x = pos.x;
			head.y = pos.y;
			head.z = pos.z + 75.f;
			Vec3 screenpos = WorldToScreen(pos, vm);
			Vec3 screenhead = WorldToScreen(head, vm);
			float height = screenhead.y - screenpos.y;
			float width = height / 2.4f;

			if (screenpos.z >= 0.01f && team != localteam && health > 0 && health < 101)
			{
				DrawBorderBox(screenpos.x - (width / 2), screenpos.y, width, height, 1);
				DrawLine(screenX / 2, screenY, screenpos.x, screenpos.y);
			}
		}
	}
}