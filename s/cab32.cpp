///////////////////////////////////////////////////////////////////////////////
// cab32.cpp

#undef _UNICODE
#undef UNICODE

#include "../misc.h"

#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <time.h>


extern "C" {
int WINAPI Cab(const HWND hwnd, LPCSTR pszCmdLine,
	       LPSTR pszOutput, const DWORD dwSize);
}

int main(int argc, char *argv[])
{
  int i, size = 0;
  for (i = 1; i < argc; i++)
    size += strlen(argv[i]) + 1;
  char *command_line = new char[size + 1];
  command_line[0] = '\0';
  for (i = 1; i < argc; i++)
  {
    strcat(command_line, argv[i]);
    strcat(command_line, " ");
  }
  char output_buffer[64 * 1024];
  int r = Cab(NULL, command_line, output_buffer, sizeof(output_buffer));
  delete [] command_line;
  if (!isatty(fileno(stdout)))
    setmode(fileno(stdout), _O_BINARY);
  printf("%s", output_buffer);
  return r;
}
