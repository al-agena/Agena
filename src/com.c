/* Lua System (LuaSys v1.8): File I/O: Serial communication.

   Written by: Nodir Temirkhodjaev, ported to Agena by awalz.

   There are no copyright remarks in Mr. Temirkhodjaev source files, accompanying files or his GIT site.

   Taken from: https://github.com/tnodir/luasys, file sys/sys_comm.c, as of July 28, 2017.

   Changes in Agena:

   The new functions `com.open` and `com.close` open or close a COM port. `com.read` and `com.write` receive or send data
   through the port.

   `com.attrib` returns the settings of a port.

   `com.init` sets defaults if only a handle is given: baud rate = 9600, byte size = 8, no parity, one stop bit. In UNIX, the
   function also accepts baud rates larger than 115,200 bits.

   Some links:

   Introduction to RS232:
   https://stackoverflow.com/questions/957337/what-is-the-difference-between-dtr-dsr-and-rts-cts-flow-control
   https://www.commfront.com/pages/3-easy-steps-to-understand-and-control-your-rs232-devices

   C:
   https://stackoverflow.com/questions/6947413/how-to-open-read-and-write-from-serial-port-in-c
   https://123a321.wordpress.com/2010/02/01/serial-port-with-mingw/

   Free terminal software:
   https://www.compuphase.com/software_termite.htm

   Virtual serial port driver for Windows:
   http://com0com.sourceforge.net/
   https://sourceforge.net/projects/com0com/ (for the download) */

/*
import com
fd := com.open('COM4');
com.init(fd, 9600, "sb1"):
com.attribs(fd):
#com.write(fd, 'hallo !');
com.read(fd):
do
   str := com.read(fd, 80)
   if str :: string then print(str) fi
od
*/

#define com_c
#define LUA_LIB

#include <fcntl.h>    /* for open */
#include <stdlib.h>
#include <unistd.h>   /* for close */

#ifndef LUA51
#include "agena.h"
#include "agnxlib.h"
#include "agenalib.h"
#else
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#endif

#include "agncmpt.h"  /* for O_* file modes */
#include "agnhlps.h"
#include "charbuf.h"
#include "luasys.h"

#define AGENA_LIBVERSION	"com 0.0.1 for Agena as of April 14, 2020\n"

#if !(defined(__DJGPP__) || defined(__OS2__) || defined(LUA_ANSI))
#define AGENA_COMLIBNAME "com"
LUALIB_API int (luaopen_com) (lua_State *L);
#endif


typedef struct Handle {
  fd_t fd;  /* in Windows HANDLE actually, in non-Windows: int */
} Handle;

static fd_t aux_getcom (lua_State *L, int idx) {
  Handle *a = (Handle *)luaL_checkudata(L, idx, "com");
  return a->fd;
}


/* Returns a handle to the given COM port, a userdata. The function does not configure the port, see `com.init`. */
static int com_open (lua_State *L) {
  Handle *a;
  int en;
  fd_t fd;
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  const char *portname = agn_checkstring(L, 1);
#ifndef _WIN32
  fd = open(portname, O_BINARY | O_RDWR | O_NOCTTY | O_SYNC);
#else
  /* fd = open(portname, O_BINARY | O_RDWR); */
  fd = CreateFile(portname, GENERIC_READ|GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
#endif
  en = errno;
  if (fd < 0) {
    luaL_error(L, "Error in " LUA_QS ": could not open port %s, %s.", "net.open", portname, my_ioerror(en));
  }
  a = (Handle *)lua_newuserdata(L, sizeof(Handle));
  luaL_getmetatable(L, "com");
  lua_setmetatable(L, -2);
  a->fd = fd;
  if (agnL_gettablefield(L, "com", "openports", "com.open", 1) == LUA_TTABLE) {
    /* DO NOT use lua_istable since it checks a value on the stack and not the return value of agn_gettablefield (a nonneg int) */
    lua_pushvalue(L, -2);  /* push userdata */
    lua_pushstring(L, portname);
    lua_rawset(L, -3);
  } else
    luaL_error(L, "Error in " LUA_QS ": 'com.openports' table not found.", "com.open");
  agn_poptop(L);  /* delete "openports" */
  return 1;
}


/* Closes the given port. */
static int com_close (lua_State *L) {
  int en;
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  lua_settop(L, 1);
  const fd_t fd = aux_getcom(L, 1);  /* const fd_t fd = (fd_t)lua_unboxinteger(L, 1, FD_TYPENAME); */
#ifndef _WIN32
  if (close(fd) == -1) {
#else
  if (CloseHandle(fd) == 0) {
#endif
    en = errno;
    luaL_error(L, "Error in " LUA_QS ": could not close port %d, %s.", "net.open", fd, my_ioerror(en));
  }
  if (agnL_gettablefield(L, "com", "openports", "com.close", 1) == LUA_TTABLE) {
    lua_pushvalue(L, 1);
    lua_pushnil(L);
    lua_rawset(L, -3);
  }  else
    luaL_error(L, "Error in " LUA_QS ": 'com.openports' table not found.", "com.close");
  agn_poptop(L);  /* delete "openports" */
  return 0;
}


/* Sends the entire string `str` to the port denoted by `hdl`. The number of bytes actually sent is returned. */
static int com_write (lua_State *L) {
  int en;
  size_t l;
#ifndef _WIN32
  size_t r;
#else
  DWORD r;
#endif
  const char *str;
  const fd_t fd = aux_getcom(L, 1);
  set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
  str = agn_checklstring(L, 2, &l);
#ifndef _WIN32
  if ( (r = write(fd, str, l)) == -1 ) {
#else
  r = 0;
  if (!WriteFile(fd, str, l, &r, NULL)) {
#endif
    en = errno;
    luaL_error(L, "Error in " LUA_QS ": could not write to port %d, %s.", "net.open", fd, my_ioerror(en));
  }
  lua_pushnumber(L, r);
  return 1;
}


/* Reads data from the port denoted by `hdl` and returns it as a string. If nothing could be read, the return is `null`.

   The (maximum) number of bytes to be read is given by `bufsize`. If not given, the function tries to read the entire
   in-queue. You should prefer to pass a value for `bufsize`. */
static int com_read (lua_State *L) {
#ifdef _WIN32
  DWORD n;
#else
  int n;
#endif
  int en, s, nbytes, readsth;
  Charbuf buf;
  char *buffer;
  int all = lua_gettop(L) == 1;
  const fd_t hnd = aux_getcom(L, 1);
#ifdef _WIN32
  n = 0;
  readsth = 0;
#endif
  /* reading a lot of bytes at one stroke will cause block size errors, so keep the buffer small */
  s = luaL_optinteger(L, 2, agn_getbuffersize(L));
  if (s == -1)
    luaL_error(L, "Error in " LUA_QS " with com(%p): I/O error.", "com.read", lua_topointer(L, 1));
  else if (s <= 0)
    s = LUAL_BUFFERSIZE;
  nbytes = s * sizeof(char);
  buffer = (char *)malloc(nbytes);
  if (buffer == NULL)  /* 4.11.5 fix */
    luaL_error(L, "Error in " LUA_QS " with com(%p): memory allocation failed.", "com.read", lua_topointer(L, 1));
  if (all) charbuf_init(L, &buf, 0, s, 1);
  do {
    set_errno(0);  /* 2.39.5 reset, better be sure than sorry, as Windows 2000 seems susceptible to uncleared errno's */
#ifndef _WIN32
    n = read(hnd, buffer, nbytes);
#else
    if (!ReadFile(hnd, buffer, s, &n, NULL)) {
      xfree(buffer);
      luaL_error(L, "Error in " LUA_QS " with com(%p): I/O error.", "com.read", lua_topointer(L, 1));
    }
#endif
    en = errno;
    if (n > 0) {
      if (all) {
        readsth = 1;
        charbuf_append(L, &buf, (const char *)buffer, n);
      } else {
        lua_pushlstring(L, buffer, n); goto readlabel;  /* bail out after one read operation */
      }
    }
#ifndef _WIN32  /* 7.9.6 DWORDs are always unsigned */
    else if (n < 0) {
      xfree(buffer);
      luaL_error(L, "Error in " LUA_QS " with port %p: %s.", "com.read", lua_topointer(L, 1), my_ioerror(en));
    }
#endif
  } while (n > 0 && all);
  if (readsth)
    lua_pushlstring(L, buf.data, buf.size);
  else
    lua_pushnil(L);
  charbuf_free(&buf);
readlabel:
  xfree(buffer);
  return 1;
}


/* Returns the settings of the given COM port, denoted by its handle hdl. The function is available in Windows only. */
#ifdef _WIN32
static int com_attrib (lua_State *L) {
  DCB dcb;
  const fd_t fd = aux_getcom(L, 1);
  dcb.DCBlength = sizeof(DCB);
  if (!GetCommState(fd, &dcb)) return sys_seterror(L, 0);
  lua_newtable(L);
  lua_rawsetstringnumber(L, -1, "baudrate", dcb.BaudRate);
  lua_rawsetstringnumber(L, -1, "fbinary", dcb.fBinary);
  lua_rawsetstringnumber(L, -1, "fparity", dcb.fParity);
  lua_rawsetstringnumber(L, -1, "foutxctsflow", dcb.fOutxCtsFlow);
  lua_rawsetstringnumber(L, -1, "foutxdsrflow", dcb.fOutxDsrFlow);
  lua_rawsetstringnumber(L, -1, "fdtrcontrol", dcb.fDtrControl);
  lua_rawsetstringnumber(L, -1, "fdsrsensitivity", dcb.fDsrSensitivity);
  lua_rawsetstringnumber(L, -1, "ftxcontinueonxoff", dcb.fTXContinueOnXoff);
  lua_rawsetstringnumber(L, -1, "foutx", dcb.fOutX);
  lua_rawsetstringnumber(L, -1, "finx", dcb.fInX);
  lua_rawsetstringnumber(L, -1, "ferrorchar", dcb.fErrorChar);
  lua_rawsetstringnumber(L, -1, "fnull", dcb.fNull);
  lua_rawsetstringnumber(L, -1, "frtscontrol", dcb.fRtsControl);
  lua_rawsetstringnumber(L, -1, "fabortonerror", dcb.fAbortOnError);
  lua_rawsetstringnumber(L, -1, "fdummy2", dcb.fDummy2);
  lua_rawsetstringnumber(L, -1, "xonlim", dcb.XonLim);
  lua_rawsetstringnumber(L, -1, "xofflim", dcb.XoffLim);
  lua_rawsetstringnumber(L, -1, "bytesize", dcb.ByteSize);
  lua_rawsetstringnumber(L, -1, "parity", dcb.Parity);
  lua_rawsetstringnumber(L, -1, "stopbits", dcb.StopBits);
  lua_rawsetstringchar(L, -1, "xonchar", dcb.XonChar);
  lua_rawsetstringchar(L, -1, "xoffchar", dcb.XoffChar);
  lua_rawsetstringchar(L, -1, "errorchar", dcb.ErrorChar);
  lua_rawsetstringchar(L, -1, "eofchar", dcb.EofChar);
  lua_rawsetstringchar(L, -1, "evtchar", dcb.EvtChar);
  return 1;
}
#endif


/* Configures the COM port denoted by its handle `hdl`. The function reinitializes all hardware and control settings, but
   it does not empty output or input queues.
   Arguments: fd_udata, [options ...:
   *	reset (string: "reset"),
   *	baud_rate (number),
   *	character_size (string: "cs5".."cs8"),
   *	parity (string: "parno", "parodd", "pareven"),
   *	stop_bits (string: "sb1", "sb2"),
   *	flow_controls (string: "foff", "frtscts", "fxio")]
   * Returns: [fd_udata]

   * In DOS, you (obviously ?) can only use baudrates 9600, 19200 and 38400. */
static int com_init (lua_State *L) {
  const fd_t fd = aux_getcom(L, 1);
  const int nargs = lua_gettop(L);
  int i;
#ifndef _WIN32
  struct termios tio;
  if (tcgetattr(fd, &tio) == -1) goto err;
#else
  DCB dcb;
  dcb.DCBlength = sizeof(DCB);
  if (!GetCommState(fd, &dcb)) goto err;
#endif
  if (nargs == 1) {  /* set defaults, new 2.18.2 */
#ifndef _WIN32
    tcflag_t mask = 0, flag = 0;
    if (cfsetispeed(&tio, B9600) == -1 || cfsetospeed(&tio, B9600) == -1) goto err;
    /* character size */
    flag = CS8;
    mask = CSIZE;
    tio.c_cflag &= ~mask;
    tio.c_cflag |= flag;
    /* parity */
    flag = 0;  /* no parity */
    mask = PARENB | PARODD;
    tio.c_cflag &= ~mask;
    tio.c_cflag |= flag;
    /* stop bits */
    flag = CSTOPB;
    mask = CSTOPB;
    tio.c_cflag &= ~mask;
    tio.c_cflag |= flag;
#else
    dcb.BaudRate = 9600;
    dcb.ByteSize = 8;
    dcb.Parity = 0;
    dcb.fParity = 0;
    dcb.StopBits = ONESTOPBIT;
#endif
#ifndef _WIN32
    tio.c_cflag |= CLOCAL | CREAD;
    if (!tcsetattr(fd, TCSANOW, &tio)) {
#else
    if (SetCommState(fd, &dcb)) {
#endif
      lua_settop(L, 1);  /* return handle */
      return 1;
    }
    return sys_seterror(L, 0);
  }
  for (i=2; i <= nargs; ++i) {
#ifndef _WIN32
    tcflag_t mask = 0, flag = 0;
#endif
    if (lua_isnumber(L, i)) {
      const int baud_rate = (int)lua_tointeger(L, i);
#ifndef _WIN32
      switch (baud_rate) {
        case 9600:    flag = B9600;    break;
        case 19200:   flag = B19200;   break;
        case 38400:   flag = B38400;   break;
        case 57600:   flag = B57600;   break;
        case 115200:  flag = B115200;  break;
        case 230400:  flag = B230400;  break;  /* from here on: 2.18.2 extension */
        case 460800:  flag = B460800;  break;
        case 500000:  flag = B500000;  break;
        case 576000:  flag = B576000;  break;
        case 921600:  flag = B921600;  break;
        case 1000000: flag = B1000000; break;
        case 1152000: flag = B1152000; break;
        case 1500000: flag = B1500000; break;
        case 2000000: flag = B2000000; break;
        case 2500000: flag = B2500000; break;
        case 3000000: flag = B3000000; break;
        case 3500000: flag = B3500000; break;
        case 4000000: flag = B4000000; break;
      }
      if (cfsetispeed(&tio, flag) == -1 || cfsetospeed(&tio, flag) == -1) goto err;
#else
      dcb.BaudRate = baud_rate;
#endif
    } else {
      const char *opt = lua_tostring(L, i);
      const char *endp = opt + lua_objlen(L, i) - 1;
      if (!opt) continue;
      switch (*opt) {
        case 'r':  /* reset */
#ifndef _WIN32
          memset(&tio, 0, sizeof(struct termios));
#else
          memset(&dcb, 0, sizeof(DCB));
#endif
          continue;
        case 'c':  /* character size */
#ifndef _WIN32
          switch (*endp) {
            case '5': flag = CS5; break;
            case '6': flag = CS6; break;
            case '7': flag = CS7; break;
            default:
              flag = CS8;
          }
          mask = CSIZE;
#else
          dcb.ByteSize = (char)(*endp - '0');
#endif
        break;
      case 'p':  /* parity */
#ifndef _WIN32
        switch (*endp) {
          case 'd': flag = PARODD;
          case 'n': flag |= PARENB; break;
          default:
            flag = 0;  /* no parity */
        }
        mask = PARENB | PARODD;
#else
        {
          int parity;
          switch (*endp) {
          case 'd': parity = ODDPARITY; break;
          case 'n': parity = EVENPARITY; break;
          default: parity = 0;  /* no parity */
          }
          dcb.Parity = (char) parity;
          dcb.fParity = (parity != 0);
        }
#endif
        break;
      case 's':  /* stop bits */
#ifndef _WIN32
        if (*endp == '2') flag = CSTOPB;  /* else: one stop bit */
        mask = CSTOPB;
#else
        dcb.StopBits = (char) (*endp == '2' ? TWOSTOPBITS : ONESTOPBIT);
#endif
        break;
      case 'f':  /* flow controls */
        /* off */
#ifndef _WIN32
        mask = CRTSCTS;
        tio.c_iflag &= ~(IXON | IXOFF | IXANY);
#else
        dcb.fOutX = dcb.fInX = 0;
        dcb.fRtsControl = RTS_CONTROL_DISABLE;
        dcb.fOutxCtsFlow = 0;
#endif
        switch (opt[1]) {
          case 'x':  /* XON/XOFF */
#ifndef _WIN32
            tio.c_iflag |= (IXON | IXOFF | IXANY);
#else
            dcb.fOutX = dcb.fInX = 1;
#endif
            break;
          case 'r':  /* RTS/CTS */
#ifndef _WIN32
            flag = CRTSCTS;
#else
            dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
            dcb.fOutxCtsFlow = 1;
#endif
            break;
        }
        break;
      }
    }
#ifndef _WIN32
    tio.c_cflag &= ~mask;
    tio.c_cflag |= flag;
#endif
  }
#ifndef _WIN32
  tio.c_cflag |= CLOCAL | CREAD;
  if (!tcsetattr(fd, TCSANOW, &tio)) {
#else
  if (SetCommState(fd, &dcb)) {
#endif
    lua_settop(L, 1);  /* return handle */
    return 1;
  }
err:
  return sys_seterror(L, 0);
}


/* Sets DTR/DSR or RTS/CTS hardware flow controls to the COM port denoted by its handle `hdl`:

   *	'dtr' - Data Terminal Ready
   *	'dsr' - Data Set Ready
   *	'rts' - Request To Send
   *	'cts' - Clear To Send

   The function does not empty output or input queues.

   Arguments: fd_udata, [controls (string: "dtr", "dsr", "rts", "cts") ...]
   Returns: [fd_udata] */
static int com_control (lua_State *L) {
  const int nargs = lua_gettop(L);
  const fd_t fd = aux_getcom(L, 1);
  int i;
#ifndef _WIN32
  int flags;
  if (ioctl(fd, TIOCMGET, &flags) == -1) goto err;
  flags &= ~(TIOCM_DTR | TIOCM_DSR | TIOCM_RTS | TIOCM_CTS
   | TIOCM_ST | TIOCM_SR | TIOCM_CAR | TIOCM_RNG);
#else
  DCB dcb;
  dcb.DCBlength = sizeof(DCB);
  if (!GetCommState(fd, &dcb)) goto err;
  dcb.fDtrControl = dcb.fOutxDsrFlow
   = dcb.fRtsControl = dcb.fOutxCtsFlow = 0;
#endif
  for (i=2; i <= nargs; ++i) {
    const char *s = lua_tostring(L, i);
    if (!s) continue;
    switch (*s) {
    case 'd':  /* DTR/DSR */
#ifndef _WIN32
      flags |= (s[1] == 't') ? TIOCM_DTR : TIOCM_DSR;
#else
      if (s[1] == 't') dcb.fDtrControl = 1;
      else dcb.fOutxDsrFlow = 1;
#endif
      break;
    case 'r':  /* RTS */
#ifndef _WIN32
      flags |= TIOCM_RTS;
#else
      dcb.fRtsControl = 1;
#endif
      break;
    case 'c':  /* CTS */
#ifndef _WIN32
      flags |= TIOCM_CTS;
#else
      dcb.fOutxCtsFlow = 1;
#endif
      break;
    }
  }
#ifndef _WIN32
  if (!ioctl(fd, TIOCMSET, &flags)) {
#else
  if (SetCommState(fd, &dcb)) {
#endif
    lua_settop(L, 1);
    return 1;
  }
err:
  return sys_seterror(L, 0);
}


/* Sets the time-out parameters for all read and write operations on the specified COM port, denoted by its handle `hdl`.
   Arguments: fd_udata, [read_timeout (milliseconds)]
   Returns: [fd_udata] */
static int com_timeout (lua_State *L) {
  const fd_t fd = aux_getcom(L, 1);
  const int rtime = (int)lua_tointeger(L, 2);
#ifndef _WIN32
  struct termios tio;
  if (tcgetattr(fd, &tio) == -1) goto err;
  tio.c_cc[VTIME] = 0.01*rtime;
  tio.c_cc[VMIN] = 0;
  if (!tcsetattr(fd, TCSANOW, &tio)) {
#else
  COMMTIMEOUTS timeouts;
  memset(&timeouts, 0, sizeof(COMMTIMEOUTS));
  timeouts.ReadIntervalTimeout = rtime ? (DWORD)rtime : MAXDWORD;
  timeouts.ReadTotalTimeoutMultiplier =
  timeouts.ReadTotalTimeoutConstant = rtime;
  if (SetCommTimeouts(fd, &timeouts)) {
#endif
    lua_settop(L, 1);
    return 1;
  }
#ifndef _WIN32
err:
#endif
  return sys_seterror(L, 0);
}


/* Initializes the communications parameters (i.e. buffer sizes for in and out queues) for the COM port denoted by its `hdl` handle.
   Arguments: fd_udata, in_buffer_size (number), out_buffer_size (number)
   Returns: [fd_udata] */
static int com_queues (lua_State *L) {
#ifndef _WIN32
  if (1) {
#else
  const fd_t fd = aux_getcom(L, 1);
  const int rqueue = (int)lua_tointeger(L, 2);
  const int wqueue = (int)lua_tointeger(L, 3);
  if (SetupComm(fd, rqueue, wqueue)) {
#endif
    lua_settop(L, 1);
    return 1;
  }
#ifdef _WIN32
  return sys_seterror(L, 0);
#endif
}


/* Discards all characters from the output or input buffer of the given COM port, denoted by it handle `hdl`. It can also terminate pending
   read or write operations on the resource."
   Arguments: fd_udata, [mode (string: "rw", "r", "w")]
   Returns: [fd_udata] */
static int com_purge (lua_State *L) {
  const fd_t fd = aux_getcom(L, 1);
  const char *mode = lua_tostring(L, 2);
  int flags;
  flags = TCIOFLUSH;
  if (mode)
    switch (mode[0]) {
    case 'w': flags = TCOFLUSH; break;
    case 'r': if (!mode[1]) flags = TCIFLUSH;
  }
#ifndef _WIN32
  if (!tcflush(fd, flags)) {
#else
  if (PurgeComm(fd, flags)) {
#endif
    lua_settop(L, 1);
    return 1;
  }
  return sys_seterror(L, 0);
}


/* Waits for an event to occur for the COM port denoted by its handle `hdl`. The set of events that are monitored by this function is
   contained in the event mask associated with the device handle."
   Arguments: fd_udata, options (string: "car", "cts", "dsr", "ring") ...
   Returns: [boolean ...] */
static int com_wait (lua_State *L) {
  const fd_t fd = aux_getcom(L, 1);
  const int nargs = lua_gettop(L);
  unsigned long status = 0;
  int flags = 0;
  int i, res;
  for (i=2; i <= nargs; ++i) {
    const char *s = lua_tostring(L, i);
    if (!s) continue;
    switch (*s) {
    case 'c':  /* CAR/CTS */
      flags |= (s[1] == 'a') ? TIOCM_CAR : TIOCM_CTS;
      break;
    case 'd':  /* DSR */
      flags |= TIOCM_DSR;
      break;
    case 'r':  /* RING */
      flags |= TIOCM_RNG;
      break;
    }
  }
  /* sys_vm_leave(L); XXX */
#ifndef _WIN32
  do {
#ifdef TIOCMIWAIT
    res = !(ioctl(fd, TIOCMIWAIT, flags) || ioctl(fd, TIOCMGET, &status));
#else
    while ((res = !ioctl(fd, TIOCMGET, &status)) && !(status & flags))
      usleep(10000);  /* 10 msec polling */
#endif
  } while (!res);  /* && sys_eintr()); */
#else
  res = SetCommMask(fd, flags) && WaitCommEvent(fd, &status, NULL);
#endif
  /* sys_vm_enter(L); XXX */
  if (res) {
    if (flags & TIOCM_CAR)
      lua_pushboolean(L, status & TIOCM_CAR);
    if (flags & TIOCM_CTS)
      lua_pushboolean(L, status & TIOCM_CTS);
    if (flags & TIOCM_DSR)
      lua_pushboolean(L, status & TIOCM_DSR);
    if (flags & TIOCM_RNG)
      lua_pushboolean(L, status & TIOCM_RNG);
    return nargs - 1;
  }
  return sys_seterror(L, 0);
}


static int mt_tostring (lua_State *L) {  /* at the console, the port is formatted as follows: */
  if (luaL_isudata(L, 1, "com"))
    lua_pushfstring(L, "com(%p)", lua_topointer(L, 1));
  else {
    void *p = lua_touserdata(L, 1);
    lua_pushfstring(L, (p != NULL) ? "userdata(%p)" : "unknown(%p)", lua_topointer(L, 1));
  }
  return 1;
}


static const struct luaL_Reg com_comlib [] = {  /* metamethods for com `n' */
  {"__gc",         com_close},          /* please do not forget garbage collection */
  {"__tostring",   mt_tostring},        /* for output at the console, e.g. print(n) */
  {NULL, NULL}
};

static const luaL_Reg comlib[] = {
#ifdef _WIN32
  {"attrib",       com_attrib},  /* renamed 2.26.2 */
#endif
  {"close",		   com_close},
  {"control",	   com_control},
  {"init",		   com_init},
  {"open",		   com_open},
  {"purge",	       com_purge},
  {"queues",	   com_queues},
  {"read",         com_read},
  {"timeout",	   com_timeout},
  {"wait",		   com_wait},
  {"write",		   com_write},
  {NULL, NULL}
};


/*
** Open com library
*/
LUALIB_API int luaopen_com (lua_State *L) {
  /* metamethods */
  luaL_newmetatable(L, "com");
  luaL_register(L, NULL, com_comlib);
  /* register library */
  luaL_register(L, AGENA_COMLIBNAME, comlib);
  lua_newtable(L);
  lua_setfield(L, -2, "openports");  /* table for information on all open ports */
  lua_rawsetstringstring(L, -1, "initstring", AGENA_LIBVERSION);
  return 1;
}

