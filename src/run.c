#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0502  /* Targets Windows XP / Server 2003 and later */
#endif

#include <windows.h>
#include <stdio.h>

typedef BOOL (WINAPI *SETDLLDIRA)(LPCSTR);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  char currentDir[MAX_PATH];
  char *newPath;
  char *oldPath;
  char libPath[MAX_PATH];
  char *commandLine;
  DWORD oldPathSize;
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  if (GetModuleFileName(NULL, currentDir, MAX_PATH) == 0) {
    return 1;
  }
  char *lastSlash = strrchr(currentDir, '\\');
  if (lastSlash != NULL) {
    *lastSlash = '\0';
  }
  HMODULE hKernel32 = GetModuleHandle("kernel32.dll");
  SETDLLDIRA pSetDllDirectoryA = (SETDLLDIRA)GetProcAddress(hKernel32, "SetDllDirectoryA");
  if (pSetDllDirectoryA != NULL) {
    /* modern Windows (XP+): use clean DLL search path */
    char binDirForDlls[MAX_PATH];
    sprintf(binDirForDlls, "%s\\bin", currentDir);
    pSetDllDirectoryA(binDirForDlls);
  } else {
    /* Windows 2000: jump physically into 'bin' folder. Since Windows 2000 is always searching 
       for DLLs in the current working directory, agena.exe will find its DLLs there. */
    char binDirForDlls[MAX_PATH];
    sprintf(binDirForDlls, "%s\\bin", currentDir);
    SetCurrentDirectory(binDirForDlls);
  }
  oldPathSize = GetEnvironmentVariable("PATH", NULL, 0);
  if (oldPathSize > 0) {
    oldPath = (char *)malloc(oldPathSize);
    if (oldPath != NULL) {
      GetEnvironmentVariable("PATH", oldPath, oldPathSize);
      newPath = (char *)malloc(strlen(currentDir) + 6 + oldPathSize + 1);
      if (newPath != NULL) {
        sprintf(newPath, "%s\\bin;%s", currentDir, oldPath);
        SetEnvironmentVariable("PATH", newPath);
        free(newPath);
      }
      free(oldPath);
    }
  }
  sprintf(libPath, "%s\\lib", currentDir);
  SetEnvironmentVariable("AGENALIBPATH", libPath);
  commandLine = (char *)malloc(19 + strlen(libPath) + 1);
  if (commandLine == NULL) {
    return 1;
  }
  sprintf(commandLine, "bin\\agena.exe -p \"%s\"", libPath);
  
  char workingDir[MAX_PATH];
  sprintf(workingDir, "%s\\bin", currentDir);  
  
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.lpTitle = "Agena Programming Language";
  ZeroMemory(&pi, sizeof(pi));
  /* Create the process without external handle manipulation */
  if (CreateProcess(NULL, commandLine, NULL, NULL, FALSE,
            CREATE_NEW_CONSOLE, NULL, workingDir, &si, &pi)) {
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
  } else {
    /* If it fails, we show a simple error to debug without crashing */
    MessageBox(NULL, "Failed to launch agena.exe", "Error", MB_OK | MB_ICONERROR);
  }
  free(commandLine);
  return 0;
}