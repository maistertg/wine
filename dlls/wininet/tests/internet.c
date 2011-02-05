/*
 * Wininet - internet tests
 *
 * Copyright 2005 Vijay Kiran Kamuju
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>
#include <string.h>
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "wininet.h"
#include "winerror.h"
#include "winreg.h"

#include "wine/test.h"

static BOOL (WINAPI *pCreateUrlCacheContainerA)(DWORD, DWORD, DWORD, DWORD,
                                                DWORD, DWORD, DWORD, DWORD);
static BOOL (WINAPI *pCreateUrlCacheContainerW)(DWORD, DWORD, DWORD, DWORD,
                                                DWORD, DWORD, DWORD, DWORD);
static BOOL (WINAPI *pInternetTimeFromSystemTimeA)(CONST SYSTEMTIME *,DWORD ,LPSTR ,DWORD);
static BOOL (WINAPI *pInternetTimeFromSystemTimeW)(CONST SYSTEMTIME *,DWORD ,LPWSTR ,DWORD);
static BOOL (WINAPI *pInternetTimeToSystemTimeA)(LPCSTR ,SYSTEMTIME *,DWORD);
static BOOL (WINAPI *pInternetTimeToSystemTimeW)(LPCWSTR ,SYSTEMTIME *,DWORD);
static BOOL (WINAPI *pIsDomainLegalCookieDomainW)(LPCWSTR, LPCWSTR);
static DWORD (WINAPI *pPrivacyGetZonePreferenceW)(DWORD, DWORD, LPDWORD, LPWSTR, LPDWORD);
static DWORD (WINAPI *pPrivacySetZonePreferenceW)(DWORD, DWORD, DWORD, LPCWSTR);

/* Win9x and WinMe don't have lstrcmpW */
static int strcmp_ww(const WCHAR *str1, const WCHAR *str2)
{
    DWORD len1 = lstrlenW(str1);
    DWORD len2 = lstrlenW(str2);

    if (len1 != len2) return 1;
    return memcmp(str1, str2, len1 * sizeof(WCHAR));
}

/* ############################### */

static void test_InternetCanonicalizeUrlA(void)
{
    CHAR    buffer[256];
    LPCSTR  url;
    DWORD   urllen;
    DWORD   dwSize;
    DWORD   res;

    /* Acrobat Updater 5 calls this for Adobe Reader 8.1 */
    url = "http://swupmf.adobe.com/manifest/50/win/AdobeUpdater.upd";
    urllen = lstrlenA(url);

    memset(buffer, '#', sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';
    dwSize = 1; /* Acrobat Updater use this size */
    SetLastError(0xdeadbeef);
    res = InternetCanonicalizeUrlA(url, buffer, &dwSize, 0);
    ok( !res && (GetLastError() == ERROR_INSUFFICIENT_BUFFER) && (dwSize == (urllen+1)),
        "got %u and %u with size %u for '%s' (%d)\n",
        res, GetLastError(), dwSize, buffer, lstrlenA(buffer));


    /* buffer has no space for the terminating '\0' */
    memset(buffer, '#', sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';
    dwSize = urllen;
    SetLastError(0xdeadbeef);
    res = InternetCanonicalizeUrlA(url, buffer, &dwSize, 0);
    /* dwSize is nr. of needed bytes with the terminating '\0' */
    ok( !res && (GetLastError() == ERROR_INSUFFICIENT_BUFFER) && (dwSize == (urllen+1)),
        "got %u and %u with size %u for '%s' (%d)\n",
        res, GetLastError(), dwSize, buffer, lstrlenA(buffer));

    /* buffer has the required size */
    memset(buffer, '#', sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';
    dwSize = urllen+1;
    SetLastError(0xdeadbeef);
    res = InternetCanonicalizeUrlA(url, buffer, &dwSize, 0);
    /* dwSize is nr. of copied bytes without the terminating '\0' */
    ok( res && (dwSize == urllen) && (lstrcmpA(url, buffer) == 0),
        "got %u and %u with size %u for '%s' (%d)\n",
        res, GetLastError(), dwSize, buffer, lstrlenA(buffer));

    memset(buffer, '#', sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';
    dwSize = sizeof(buffer);
    SetLastError(0xdeadbeef);
    res = InternetCanonicalizeUrlA("file:///C:/Program%20Files/Atmel/AVR%20Tools/STK500/STK500.xml", buffer, &dwSize, ICU_DECODE | ICU_NO_ENCODE);
    ok(res, "InternetCanonicalizeUrlA failed %u\n", GetLastError());
    ok(dwSize == lstrlenA(buffer), "got %d expected %d\n", dwSize, lstrlenA(buffer));
    ok(!lstrcmpA("file://C:\\Program Files\\Atmel\\AVR Tools\\STK500\\STK500.xml", buffer),
       "got %s expected 'file://C:\\Program Files\\Atmel\\AVR Tools\\STK500\\STK500.xml'\n", buffer);

    /* buffer is larger as the required size */
    memset(buffer, '#', sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = '\0';
    dwSize = urllen+2;
    SetLastError(0xdeadbeef);
    res = InternetCanonicalizeUrlA(url, buffer, &dwSize, 0);
    /* dwSize is nr. of copied bytes without the terminating '\0' */
    ok( res && (dwSize == urllen) && (lstrcmpA(url, buffer) == 0),
        "got %u and %u with size %u for '%s' (%d)\n",
        res, GetLastError(), dwSize, buffer, lstrlenA(buffer));


    /* check NULL pointers */
    memset(buffer, '#', urllen + 4);
    buffer[urllen + 4] = '\0';
    dwSize = urllen+1;
    SetLastError(0xdeadbeef);
    res = InternetCanonicalizeUrlA(NULL, buffer, &dwSize, 0);
    ok( !res && (GetLastError() == ERROR_INVALID_PARAMETER),
        "got %u and %u with size %u for '%s' (%d)\n",
        res, GetLastError(), dwSize, buffer, lstrlenA(buffer));

    memset(buffer, '#', urllen + 4);
    buffer[urllen + 4] = '\0';
    dwSize = urllen+1;
    SetLastError(0xdeadbeef);
    res = InternetCanonicalizeUrlA(url, NULL, &dwSize, 0);
    ok( !res && (GetLastError() == ERROR_INVALID_PARAMETER),
        "got %u and %u with size %u for '%s' (%d)\n",
        res, GetLastError(), dwSize, buffer, lstrlenA(buffer));

    memset(buffer, '#', urllen + 4);
    buffer[urllen + 4] = '\0';
    dwSize = urllen+1;
    SetLastError(0xdeadbeef);
    res = InternetCanonicalizeUrlA(url, buffer, NULL, 0);
    ok( !res && (GetLastError() == ERROR_INVALID_PARAMETER),
        "got %u and %u with size %u for '%s' (%d)\n",
        res, GetLastError(), dwSize, buffer, lstrlenA(buffer));

    /* test with trailing space */
    dwSize = 256;
    res = InternetCanonicalizeUrlA("http://www.winehq.org/index.php?x= ", buffer, &dwSize, ICU_BROWSER_MODE);
    ok(res == 1, "InternetCanonicalizeUrlA failed\n");
    ok(!strcmp(buffer, "http://www.winehq.org/index.php?x="), "Trailing space should have been stripped even in ICU_BROWSER_MODE (%s)\n", buffer);

    res = InternetSetOptionA(NULL, 0xdeadbeef, buffer, sizeof(buffer));
    ok(!res, "InternetSetOptionA succeeded\n");
    ok(GetLastError() == ERROR_INTERNET_INVALID_OPTION,
       "InternetSetOptionA failed %u, expected ERROR_INTERNET_INVALID_OPTION\n", GetLastError());
}

/* ############################### */

static void test_InternetQueryOptionA(void)
{
  HINTERNET hinet,hurl;
  DWORD len, val;
  DWORD err;
  static const char useragent[] = {"Wininet Test"};
  char *buffer;
  int retval;

  hinet = InternetOpenA(useragent,INTERNET_OPEN_TYPE_DIRECT,NULL,NULL, 0);
  ok((hinet != 0x0),"InternetOpen Failed\n");

  SetLastError(0xdeadbeef);
  retval=InternetQueryOptionA(NULL,INTERNET_OPTION_USER_AGENT,NULL,&len);
  err=GetLastError();
  ok(retval == 0,"Got wrong return value %d\n",retval);
  ok(err == ERROR_INTERNET_INCORRECT_HANDLE_TYPE, "Got wrong error code%d\n",err);

  SetLastError(0xdeadbeef);
  len=strlen(useragent)+1;
  retval=InternetQueryOptionA(hinet,INTERNET_OPTION_USER_AGENT,NULL,&len);
  err=GetLastError();
  ok(len == strlen(useragent)+1,"Got wrong user agent length %d instead of %d\n",len,lstrlenA(useragent));
  ok(retval == 0,"Got wrong return value %d\n",retval);
  ok(err == ERROR_INSUFFICIENT_BUFFER, "Got wrong error code %d\n",err);

  SetLastError(0xdeadbeef);
  len=strlen(useragent)+1;
  buffer=HeapAlloc(GetProcessHeap(),0,len);
  retval=InternetQueryOptionA(hinet,INTERNET_OPTION_USER_AGENT,buffer,&len);
  err=GetLastError();
  ok(retval == 1,"Got wrong return value %d\n",retval);
  if (retval)
  {
      ok(!strcmp(useragent,buffer),"Got wrong user agent string %s instead of %s\n",buffer,useragent);
      ok(len == strlen(useragent),"Got wrong user agent length %d instead of %d\n",len,lstrlenA(useragent));
  }
  ok(err == 0xdeadbeef, "Got wrong error code %d\n",err);
  HeapFree(GetProcessHeap(),0,buffer);

  SetLastError(0xdeadbeef);
  len=0;
  buffer=HeapAlloc(GetProcessHeap(),0,100);
  retval=InternetQueryOptionA(hinet,INTERNET_OPTION_USER_AGENT,buffer,&len);
  err=GetLastError();
  ok(len == strlen(useragent) + 1,"Got wrong user agent length %d instead of %d\n", len, lstrlenA(useragent) + 1);
  ok(!retval, "Got wrong return value %d\n", retval);
  ok(err == ERROR_INSUFFICIENT_BUFFER, "Got wrong error code %d\n", err);
  HeapFree(GetProcessHeap(),0,buffer);

  hurl = InternetConnectA(hinet,"www.winehq.org",INTERNET_DEFAULT_HTTP_PORT,NULL,NULL,INTERNET_SERVICE_HTTP,0,0);

  SetLastError(0xdeadbeef);
  len=0;
  retval = InternetQueryOptionA(hurl,INTERNET_OPTION_USER_AGENT,NULL,&len);
  err=GetLastError();
  ok(len == 0,"Got wrong user agent length %d instead of 0\n",len);
  ok(retval == 0,"Got wrong return value %d\n",retval);
  ok(err == ERROR_INTERNET_INCORRECT_HANDLE_TYPE, "Got wrong error code %d\n",err);

  InternetCloseHandle(hurl);
  InternetCloseHandle(hinet);

  hinet = InternetOpenA("",INTERNET_OPEN_TYPE_DIRECT,NULL,NULL, 0);
  ok((hinet != 0x0),"InternetOpen Failed\n");

  SetLastError(0xdeadbeef);
  len=0;
  retval=InternetQueryOptionA(hinet,INTERNET_OPTION_USER_AGENT,NULL,&len);
  err=GetLastError();
  ok(len == 1,"Got wrong user agent length %d instead of %d\n",len,1);
  ok(retval == 0,"Got wrong return value %d\n",retval);
  ok(err == ERROR_INSUFFICIENT_BUFFER, "Got wrong error code%d\n",err);

  InternetCloseHandle(hinet);

  hinet = InternetOpenA(NULL,INTERNET_OPEN_TYPE_DIRECT,NULL,NULL, 0);
  ok((hinet != 0x0),"InternetOpen Failed\n");
  SetLastError(0xdeadbeef);
  len=0;
  retval=InternetQueryOptionA(hinet,INTERNET_OPTION_USER_AGENT,NULL,&len);
  err=GetLastError();
  ok(len == 1,"Got wrong user agent length %d instead of %d\n",len,1);
  ok(retval == 0,"Got wrong return value %d\n",retval);
  ok(err == ERROR_INSUFFICIENT_BUFFER, "Got wrong error code%d\n",err);

  InternetCloseHandle(hinet);

  len = sizeof(val);
  retval = InternetQueryOptionA(NULL, INTERNET_OPTION_MAX_CONNS_PER_SERVER, &val, &len);
  ok(retval == TRUE,"Got wrong return value %d\n", retval);
  ok(len == sizeof(val), "got %d\n", len);
  ok(val == 2, "got %d\n", val);

  len = sizeof(val);
  retval = InternetQueryOptionA(NULL, INTERNET_OPTION_MAX_CONNS_PER_1_0_SERVER, &val, &len);
  ok(retval == TRUE,"Got wrong return value %d\n", retval);
  ok(len == sizeof(val), "got %d\n", len);
  ok(val == 4, "got %d\n", val);

}

static void test_get_cookie(void)
{
  DWORD len;
  BOOL ret;

  SetLastError(0xdeadbeef);
  ret = InternetGetCookie("http://www.example.com", NULL, NULL, &len);
  ok(!ret && GetLastError() == ERROR_NO_MORE_ITEMS,
    "InternetGetCookie should have failed with %s and error %d\n",
    ret ? "TRUE" : "FALSE", GetLastError());
}


static void test_complicated_cookie(void)
{
  DWORD len;
  BOOL ret;

  CHAR buffer[1024];

  ret = InternetSetCookie("http://www.example.com/bar",NULL,"A=B; domain=.example.com");
  ok(ret == TRUE,"InternetSetCookie failed\n");
  ret = InternetSetCookie("http://www.example.com/bar",NULL,"C=D; domain=.example.com; path=/");
  ok(ret == TRUE,"InternetSetCookie failed\n");

  /* Technically illegal! domain should require 2 dots, but native wininet accepts it */
  ret = InternetSetCookie("http://www.example.com",NULL,"E=F; domain=example.com");
  ok(ret == TRUE,"InternetSetCookie failed\n");
  ret = InternetSetCookie("http://www.example.com",NULL,"G=H; domain=.example.com; path=/foo");
  ok(ret == TRUE,"InternetSetCookie failed\n");
  ret = InternetSetCookie("http://www.example.com/bar.html",NULL,"I=J; domain=.example.com");
  ok(ret == TRUE,"InternetSetCookie failed\n");
  ret = InternetSetCookie("http://www.example.com/bar/",NULL,"K=L; domain=.example.com");
  ok(ret == TRUE,"InternetSetCookie failed\n");
  ret = InternetSetCookie("http://www.example.com/bar/",NULL,"M=N; domain=.example.com; path=/foo/");
  ok(ret == TRUE,"InternetSetCookie failed\n");
  ret = InternetSetCookie("http://www.example.com/bar/",NULL,"O=P; secure; path=/bar");
  ok(ret == TRUE,"InternetSetCookie failed\n");

  len = 1024;
  ret = InternetGetCookie("http://testing.example.com", NULL, buffer, &len);
  ok(ret == TRUE,"InternetGetCookie failed\n");
  ok(strstr(buffer,"A=B")!=NULL,"A=B missing\n");
  ok(strstr(buffer,"C=D")!=NULL,"C=D missing\n");
  ok(strstr(buffer,"E=F")!=NULL,"E=F missing\n");
  ok(strstr(buffer,"G=H")==NULL,"G=H present\n");
  ok(strstr(buffer,"I=J")!=NULL,"I=J missing\n");
  ok(strstr(buffer,"K=L")==NULL,"K=L present\n");
  ok(strstr(buffer,"M=N")==NULL,"M=N present\n");
  ok(strstr(buffer,"O=P")==NULL,"O=P present\n");

  len = 1024;
  ret = InternetGetCookie("http://testing.example.com/foobar", NULL, buffer, &len);
  ok(ret == TRUE,"InternetGetCookie failed\n");
  ok(strstr(buffer,"A=B")!=NULL,"A=B missing\n");
  ok(strstr(buffer,"C=D")!=NULL,"C=D missing\n");
  ok(strstr(buffer,"E=F")!=NULL,"E=F missing\n");
  ok(strstr(buffer,"G=H")==NULL,"G=H present\n");
  ok(strstr(buffer,"I=J")!=NULL,"I=J missing\n");
  ok(strstr(buffer,"K=L")==NULL,"K=L present\n");
  ok(strstr(buffer,"M=N")==NULL,"M=N present\n");
  ok(strstr(buffer,"O=P")==NULL,"O=P present\n");

  len = 1024;
  ret = InternetGetCookie("http://testing.example.com/foobar/", NULL, buffer, &len);
  ok(ret == TRUE,"InternetGetCookie failed\n");
  ok(strstr(buffer,"A=B")!=NULL,"A=B missing\n");
  ok(strstr(buffer,"C=D")!=NULL,"C=D missing\n");
  ok(strstr(buffer,"E=F")!=NULL,"E=F missing\n");
  ok(strstr(buffer,"G=H")!=NULL,"G=H missing\n");
  ok(strstr(buffer,"I=J")!=NULL,"I=J missing\n");
  ok(strstr(buffer,"K=L")==NULL,"K=L present\n");
  ok(strstr(buffer,"M=N")==NULL,"M=N present\n");
  ok(strstr(buffer,"O=P")==NULL,"O=P present\n");

  len = 1024;
  ret = InternetGetCookie("http://testing.example.com/foo/bar", NULL, buffer, &len);
  ok(ret == TRUE,"InternetGetCookie failed\n");
  ok(strstr(buffer,"A=B")!=NULL,"A=B missing\n");
  ok(strstr(buffer,"C=D")!=NULL,"C=D missing\n");
  ok(strstr(buffer,"E=F")!=NULL,"E=F missing\n");
  ok(strstr(buffer,"G=H")!=NULL,"G=H missing\n");
  ok(strstr(buffer,"I=J")!=NULL,"I=J missing\n");
  ok(strstr(buffer,"K=L")==NULL,"K=L present\n");
  ok(strstr(buffer,"M=N")!=NULL,"M=N missing\n");
  ok(strstr(buffer,"O=P")==NULL,"O=P present\n");

  len = 1024;
  ret = InternetGetCookie("http://testing.example.com/barfoo", NULL, buffer, &len);
  ok(ret == TRUE,"InternetGetCookie failed\n");
  ok(strstr(buffer,"A=B")!=NULL,"A=B missing\n");
  ok(strstr(buffer,"C=D")!=NULL,"C=D missing\n");
  ok(strstr(buffer,"E=F")!=NULL,"E=F missing\n");
  ok(strstr(buffer,"G=H")==NULL,"G=H present\n");
  ok(strstr(buffer,"I=J")!=NULL,"I=J missing\n");
  ok(strstr(buffer,"K=L")==NULL,"K=L present\n");
  ok(strstr(buffer,"M=N")==NULL,"M=N present\n");
  ok(strstr(buffer,"O=P")==NULL,"O=P present\n");

  len = 1024;
  ret = InternetGetCookie("http://testing.example.com/barfoo/", NULL, buffer, &len);
  ok(ret == TRUE,"InternetGetCookie failed\n");
  ok(strstr(buffer,"A=B")!=NULL,"A=B missing\n");
  ok(strstr(buffer,"C=D")!=NULL,"C=D missing\n");
  ok(strstr(buffer,"E=F")!=NULL,"E=F missing\n");
  ok(strstr(buffer,"G=H")==NULL,"G=H present\n");
  ok(strstr(buffer,"I=J")!=NULL,"I=J missing\n");
  ok(strstr(buffer,"K=L")==NULL,"K=L present\n");
  ok(strstr(buffer,"M=N")==NULL,"M=N present\n");
  ok(strstr(buffer,"O=P")==NULL,"O=P present\n");

  len = 1024;
  ret = InternetGetCookie("http://testing.example.com/bar/foo", NULL, buffer, &len);
  ok(ret == TRUE,"InternetGetCookie failed\n");
  ok(strstr(buffer,"A=B")!=NULL,"A=B missing\n");
  ok(strstr(buffer,"C=D")!=NULL,"C=D missing\n");
  ok(strstr(buffer,"E=F")!=NULL,"E=F missing\n");
  ok(strstr(buffer,"G=H")==NULL,"G=H present\n");
  ok(strstr(buffer,"I=J")!=NULL,"I=J missing\n");
  ok(strstr(buffer,"K=L")!=NULL,"K=L missing\n");
  ok(strstr(buffer,"M=N")==NULL,"M=N present\n");
  ok(strstr(buffer,"O=P")==NULL,"O=P present\n");
}

static void test_null(void)
{
  HINTERNET hi, hc;
  static const WCHAR szServer[] = { 's','e','r','v','e','r',0 };
  static const WCHAR szEmpty[] = { 0 };
  static const WCHAR szUrl[] = { 'h','t','t','p',':','/','/','a','.','b','.','c',0 };
  static const WCHAR szUrlEmpty[] = { 'h','t','t','p',':','/','/',0 };
  static const WCHAR szExpect[] = { 's','e','r','v','e','r',';',' ','s','e','r','v','e','r',0 };
  WCHAR buffer[0x20];
  BOOL r;
  DWORD sz;

  SetLastError(0xdeadbeef);
  hi = InternetOpenW(NULL, 0, NULL, NULL, 0);
  if (hi == NULL && GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)
  {
    win_skip("Internet*W functions are not implemented\n");
    return;
  }
  ok(hi != NULL, "open failed\n");

  hc = InternetConnectW(hi, NULL, 0, NULL, NULL, 0, 0, 0);
  ok(GetLastError() == ERROR_INVALID_PARAMETER, "wrong error\n");
  ok(hc == NULL, "connect failed\n");

  hc = InternetConnectW(hi, NULL, 0, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
  ok(GetLastError() == ERROR_INVALID_PARAMETER, "wrong error\n");
  ok(hc == NULL, "connect failed\n");

  hc = InternetConnectW(hi, NULL, 0, NULL, NULL, INTERNET_SERVICE_FTP, 0, 0);
  ok(GetLastError() == ERROR_INVALID_PARAMETER, "wrong error\n");
  ok(hc == NULL, "connect failed\n");

  hc = InternetConnectW(NULL, szServer, 0, NULL, NULL, INTERNET_SERVICE_FTP, 0, 0);
  ok(GetLastError() == ERROR_INVALID_HANDLE, "wrong error\n");
  ok(hc == NULL, "connect failed\n");

  hc = InternetOpenUrlW(hi, NULL, NULL, 0, 0, 0);
  ok(GetLastError() == ERROR_INVALID_PARAMETER ||
     GetLastError() == ERROR_INTERNET_UNRECOGNIZED_SCHEME, "wrong error\n");
  ok(hc == NULL, "connect failed\n");

  hc = InternetOpenUrlW(hi, szServer, NULL, 0, 0, 0);
  ok(GetLastError() == ERROR_INTERNET_UNRECOGNIZED_SCHEME, "wrong error\n");
  ok(hc == NULL, "connect failed\n");

  InternetCloseHandle(hi);

  r = InternetSetCookieW(NULL, NULL, NULL);
  ok(GetLastError() == ERROR_INVALID_PARAMETER, "wrong error\n");
  ok(r == FALSE, "return wrong\n");

  r = InternetSetCookieW(szServer, NULL, NULL);
  ok(GetLastError() == ERROR_INVALID_PARAMETER, "wrong error\n");
  ok(r == FALSE, "return wrong\n");

  r = InternetSetCookieW(szUrl, szServer, NULL);
  ok(GetLastError() == ERROR_INVALID_PARAMETER, "wrong error\n");
  ok(r == FALSE, "return wrong\n");

  r = InternetSetCookieW(szUrl, szServer, szServer);
  ok(r == TRUE, "return wrong\n");

  r = InternetSetCookieW(szUrl, NULL, szServer);
  ok(r == TRUE, "return wrong\n");

  r = InternetSetCookieW(szUrl, szServer, szEmpty);
  ok(r == TRUE, "return wrong\n");

  r = InternetSetCookieW(szUrlEmpty, szServer, szServer);
  ok(r == FALSE, "return wrong\n");

  r = InternetSetCookieW(szServer, NULL, szServer);
  todo_wine {
  ok(GetLastError() == ERROR_INTERNET_UNRECOGNIZED_SCHEME, "wrong error\n");
  }
  ok(r == FALSE, "return wrong\n");

  sz = 0;
  r = InternetGetCookieW(NULL, NULL, NULL, &sz);
  ok(GetLastError() == ERROR_INVALID_PARAMETER || GetLastError() == ERROR_INTERNET_UNRECOGNIZED_SCHEME,
     "wrong error %u\n", GetLastError());
  ok( r == FALSE, "return wrong\n");

  r = InternetGetCookieW(szServer, NULL, NULL, &sz);
  todo_wine {
  ok(GetLastError() == ERROR_INTERNET_UNRECOGNIZED_SCHEME, "wrong error\n");
  }
  ok( r == FALSE, "return wrong\n");

  sz = 0;
  r = InternetGetCookieW(szUrlEmpty, szServer, NULL, &sz);
  ok( r == FALSE, "return wrong\n");

  sz = 0;
  r = InternetGetCookieW(szUrl, szServer, NULL, &sz);
  ok( r == TRUE, "return wrong\n");

  /* sz is 14 on XP SP2 and beyond, 30 on XP SP1 and before */
  ok( sz == 14 || sz == 30, "sz wrong, got %u, expected 14 or 30\n", sz);

  sz = 0x20;
  memset(buffer, 0, sizeof buffer);
  r = InternetGetCookieW(szUrl, szServer, buffer, &sz);
  ok( r == TRUE, "return wrong\n");

  /* sz == lstrlenW(buffer) only in XP SP1 */
  ok( sz == 1 + lstrlenW(buffer) || sz == lstrlenW(buffer), "sz wrong %d\n", sz);

  /* before XP SP2, buffer is "server; server" */
  ok( !strcmp_ww(szExpect, buffer) || !strcmp_ww(szServer, buffer), "cookie data wrong\n");

  sz = sizeof(buffer);
  r = InternetQueryOptionA(NULL, INTERNET_OPTION_CONNECTED_STATE, buffer, &sz);
  ok(r == TRUE, "ret %d\n", r);
}

static void test_version(void)
{
    INTERNET_VERSION_INFO version;
    DWORD size;
    BOOL res;

    size = sizeof(version);
    res = InternetQueryOptionA(NULL, INTERNET_OPTION_VERSION, &version, &size);
    ok(res, "Could not get version: %u\n", GetLastError());
    ok(version.dwMajorVersion == 1, "dwMajorVersion=%d, expected 1\n", version.dwMajorVersion);
    ok(version.dwMinorVersion == 2, "dwMinorVersion=%d, expected 2\n", version.dwMinorVersion);
}

static void InternetTimeFromSystemTimeA_test(void)
{
    BOOL ret;
    static const SYSTEMTIME time = { 2005, 1, 5, 7, 12, 6, 35, 0 };
    char string[INTERNET_RFC1123_BUFSIZE];
    static const char expect[] = "Fri, 07 Jan 2005 12:06:35 GMT";
    DWORD error;

    ret = pInternetTimeFromSystemTimeA( &time, INTERNET_RFC1123_FORMAT, string, sizeof(string) );
    ok( ret, "InternetTimeFromSystemTimeA failed (%u)\n", GetLastError() );

    ok( !memcmp( string, expect, sizeof(expect) ),
        "InternetTimeFromSystemTimeA failed (%u)\n", GetLastError() );

    /* test NULL time parameter */
    SetLastError(0xdeadbeef);
    ret = pInternetTimeFromSystemTimeA( NULL, INTERNET_RFC1123_FORMAT, string, sizeof(string) );
    error = GetLastError();
    ok( !ret, "InternetTimeFromSystemTimeA should have returned FALSE\n" );
    ok( error == ERROR_INVALID_PARAMETER,
        "InternetTimeFromSystemTimeA failed with ERROR_INVALID_PARAMETER instead of %u\n",
        error );

    /* test NULL string parameter */
    SetLastError(0xdeadbeef);
    ret = pInternetTimeFromSystemTimeA( &time, INTERNET_RFC1123_FORMAT, NULL, sizeof(string) );
    error = GetLastError();
    ok( !ret, "InternetTimeFromSystemTimeA should have returned FALSE\n" );
    ok( error == ERROR_INVALID_PARAMETER,
        "InternetTimeFromSystemTimeA failed with ERROR_INVALID_PARAMETER instead of %u\n",
        error );

    /* test invalid format parameter */
    SetLastError(0xdeadbeef);
    ret = pInternetTimeFromSystemTimeA( &time, INTERNET_RFC1123_FORMAT + 1, string, sizeof(string) );
    error = GetLastError();
    ok( !ret, "InternetTimeFromSystemTimeA should have returned FALSE\n" );
    ok( error == ERROR_INVALID_PARAMETER,
        "InternetTimeFromSystemTimeA failed with ERROR_INVALID_PARAMETER instead of %u\n",
        error );

    /* test too small buffer size */
    SetLastError(0xdeadbeef);
    ret = pInternetTimeFromSystemTimeA( &time, INTERNET_RFC1123_FORMAT, string, 0 );
    error = GetLastError();
    ok( !ret, "InternetTimeFromSystemTimeA should have returned FALSE\n" );
    ok( error == ERROR_INSUFFICIENT_BUFFER,
        "InternetTimeFromSystemTimeA failed with ERROR_INSUFFICIENT_BUFFER instead of %u\n",
        error );
}

static void InternetTimeFromSystemTimeW_test(void)
{
    BOOL ret;
    static const SYSTEMTIME time = { 2005, 1, 5, 7, 12, 6, 35, 0 };
    WCHAR string[INTERNET_RFC1123_BUFSIZE + 1];
    static const WCHAR expect[] = { 'F','r','i',',',' ','0','7',' ','J','a','n',' ','2','0','0','5',' ',
                                    '1','2',':','0','6',':','3','5',' ','G','M','T',0 };
    DWORD error;

    ret = pInternetTimeFromSystemTimeW( &time, INTERNET_RFC1123_FORMAT, string, sizeof(string) );
    ok( ret, "InternetTimeFromSystemTimeW failed (%u)\n", GetLastError() );

    ok( !memcmp( string, expect, sizeof(expect) ),
        "InternetTimeFromSystemTimeW failed (%u)\n", GetLastError() );

    /* test NULL time parameter */
    SetLastError(0xdeadbeef);
    ret = pInternetTimeFromSystemTimeW( NULL, INTERNET_RFC1123_FORMAT, string, sizeof(string) );
    error = GetLastError();
    ok( !ret, "InternetTimeFromSystemTimeW should have returned FALSE\n" );
    ok( error == ERROR_INVALID_PARAMETER,
        "InternetTimeFromSystemTimeW failed with ERROR_INVALID_PARAMETER instead of %u\n",
        error );

    /* test NULL string parameter */
    SetLastError(0xdeadbeef);
    ret = pInternetTimeFromSystemTimeW( &time, INTERNET_RFC1123_FORMAT, NULL, sizeof(string) );
    error = GetLastError();
    ok( !ret, "InternetTimeFromSystemTimeW should have returned FALSE\n" );
    ok( error == ERROR_INVALID_PARAMETER,
        "InternetTimeFromSystemTimeW failed with ERROR_INVALID_PARAMETER instead of %u\n",
        error );

    /* test invalid format parameter */
    SetLastError(0xdeadbeef);
    ret = pInternetTimeFromSystemTimeW( &time, INTERNET_RFC1123_FORMAT + 1, string, sizeof(string) );
    error = GetLastError();
    ok( !ret, "InternetTimeFromSystemTimeW should have returned FALSE\n" );
    ok( error == ERROR_INVALID_PARAMETER,
        "InternetTimeFromSystemTimeW failed with ERROR_INVALID_PARAMETER instead of %u\n",
        error );

    /* test too small buffer size */
    SetLastError(0xdeadbeef);
    ret = pInternetTimeFromSystemTimeW( &time, INTERNET_RFC1123_FORMAT, string, sizeof(string)/sizeof(string[0]) );
    error = GetLastError();
    ok( !ret, "InternetTimeFromSystemTimeW should have returned FALSE\n" );
    ok( error == ERROR_INSUFFICIENT_BUFFER,
        "InternetTimeFromSystemTimeW failed with ERROR_INSUFFICIENT_BUFFER instead of %u\n",
        error );
}

static void InternetTimeToSystemTimeA_test(void)
{
    BOOL ret;
    SYSTEMTIME time;
    static const SYSTEMTIME expect = { 2005, 1, 5, 7, 12, 6, 35, 0 };
    static const char string[] = "Fri, 07 Jan 2005 12:06:35 GMT";
    static const char string2[] = " fri 7 jan 2005 12 06 35";

    ret = pInternetTimeToSystemTimeA( string, &time, 0 );
    ok( ret, "InternetTimeToSystemTimeA failed (%u)\n", GetLastError() );
    ok( !memcmp( &time, &expect, sizeof(expect) ),
        "InternetTimeToSystemTimeA failed (%u)\n", GetLastError() );

    ret = pInternetTimeToSystemTimeA( string2, &time, 0 );
    ok( ret, "InternetTimeToSystemTimeA failed (%u)\n", GetLastError() );
    ok( !memcmp( &time, &expect, sizeof(expect) ),
        "InternetTimeToSystemTimeA failed (%u)\n", GetLastError() );
}

static void InternetTimeToSystemTimeW_test(void)
{
    BOOL ret;
    SYSTEMTIME time;
    static const SYSTEMTIME expect = { 2005, 1, 5, 7, 12, 6, 35, 0 };
    static const WCHAR string[] = { 'F','r','i',',',' ','0','7',' ','J','a','n',' ','2','0','0','5',' ',
                                    '1','2',':','0','6',':','3','5',' ','G','M','T',0 };
    static const WCHAR string2[] = { ' ','f','r','i',' ','7',' ','j','a','n',' ','2','0','0','5',' ',
                                     '1','2',' ','0','6',' ','3','5',0 };
    static const WCHAR string3[] = { 'F','r',0 };

    ret = pInternetTimeToSystemTimeW( NULL, NULL, 0 );
    ok( !ret, "InternetTimeToSystemTimeW succeeded (%u)\n", GetLastError() );

    ret = pInternetTimeToSystemTimeW( NULL, &time, 0 );
    ok( !ret, "InternetTimeToSystemTimeW succeeded (%u)\n", GetLastError() );

    ret = pInternetTimeToSystemTimeW( string, NULL, 0 );
    ok( !ret, "InternetTimeToSystemTimeW succeeded (%u)\n", GetLastError() );

    ret = pInternetTimeToSystemTimeW( string, &time, 0 );
    ok( ret, "InternetTimeToSystemTimeW failed (%u)\n", GetLastError() );

    ret = pInternetTimeToSystemTimeW( string, &time, 0 );
    ok( ret, "InternetTimeToSystemTimeW failed (%u)\n", GetLastError() );
    ok( !memcmp( &time, &expect, sizeof(expect) ),
        "InternetTimeToSystemTimeW failed (%u)\n", GetLastError() );

    ret = pInternetTimeToSystemTimeW( string2, &time, 0 );
    ok( ret, "InternetTimeToSystemTimeW failed (%u)\n", GetLastError() );
    ok( !memcmp( &time, &expect, sizeof(expect) ),
        "InternetTimeToSystemTimeW failed (%u)\n", GetLastError() );

    ret = pInternetTimeToSystemTimeW( string3, &time, 0 );
    ok( ret, "InternetTimeToSystemTimeW failed (%u)\n", GetLastError() );
}

static void test_IsDomainLegalCookieDomainW(void)
{
    BOOL ret;
    DWORD error;
    static const WCHAR empty[]          = {0};
    static const WCHAR dot[]            = {'.',0};
    static const WCHAR uk[]             = {'u','k',0};
    static const WCHAR com[]            = {'c','o','m',0};
    static const WCHAR dot_com[]        = {'.','c','o','m',0};
    static const WCHAR gmail_com[]      = {'g','m','a','i','l','.','c','o','m',0};
    static const WCHAR dot_gmail_com[]  = {'.','g','m','a','i','l','.','c','o','m',0};
    static const WCHAR mail_gmail_com[] = {'m','a','i','l','.','g','m','a','i','l','.','c','o','m',0};
    static const WCHAR gmail_co_uk[]    = {'g','m','a','i','l','.','c','o','.','u','k',0};
    static const WCHAR co_uk[]          = {'c','o','.','u','k',0};
    static const WCHAR dot_co_uk[]      = {'.','c','o','.','u','k',0};

    SetLastError(0xdeadbeef);
    ret = pIsDomainLegalCookieDomainW(NULL, NULL);
    error = GetLastError();
    if (!ret && error == ERROR_CALL_NOT_IMPLEMENTED)
    {
        win_skip("IsDomainLegalCookieDomainW is not implemented\n");
        return;
    }
    ok(!ret ||
        broken(ret), /* IE6 */
        "IsDomainLegalCookieDomainW succeeded\n");
    ok(error == ERROR_INVALID_PARAMETER, "got %u expected ERROR_INVALID_PARAMETER\n", error);

    SetLastError(0xdeadbeef);
    ret = pIsDomainLegalCookieDomainW(com, NULL);
    error = GetLastError();
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");
    ok(error == ERROR_INVALID_PARAMETER, "got %u expected ERROR_INVALID_PARAMETER\n", error);

    SetLastError(0xdeadbeef);
    ret = pIsDomainLegalCookieDomainW(NULL, gmail_com);
    error = GetLastError();
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");
    ok(error == ERROR_INVALID_PARAMETER, "got %u expected ERROR_INVALID_PARAMETER\n", error);

    SetLastError(0xdeadbeef);
    ret = pIsDomainLegalCookieDomainW(empty, gmail_com);
    error = GetLastError();
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");
    ok(error == ERROR_INVALID_NAME ||
        broken(error == ERROR_INVALID_PARAMETER), /* IE6 */
        "got %u expected ERROR_INVALID_NAME\n", error);

    SetLastError(0xdeadbeef);
    ret = pIsDomainLegalCookieDomainW(com, empty);
    error = GetLastError();
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");
    ok(error == ERROR_INVALID_NAME ||
        broken(error == ERROR_INVALID_PARAMETER), /* IE6 */
        "got %u expected ERROR_INVALID_NAME\n", error);

    SetLastError(0xdeadbeef);
    ret = pIsDomainLegalCookieDomainW(gmail_com, dot);
    error = GetLastError();
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");
    ok(error == ERROR_INVALID_NAME ||
        broken(error == 0xdeadbeef), /* IE6 */
        "got %u expected ERROR_INVALID_NAME\n", error);

    SetLastError(0xdeadbeef);
    ret = pIsDomainLegalCookieDomainW(dot, gmail_com);
    error = GetLastError();
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");
    ok(error == ERROR_INVALID_NAME ||
        broken(error == 0xdeadbeef), /* IE6 */
        "got %u expected ERROR_INVALID_NAME\n", error);

    SetLastError(0xdeadbeef);
    ret = pIsDomainLegalCookieDomainW(com, com);
    error = GetLastError();
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");
    ok(error == 0xdeadbeef, "got %u expected 0xdeadbeef\n", error);

    SetLastError(0xdeadbeef);
    ret = pIsDomainLegalCookieDomainW(com, dot_com);
    error = GetLastError();
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");
    ok(error == ERROR_INVALID_NAME ||
        broken(error == 0xdeadbeef), /* IE6 */
        "got %u expected ERROR_INVALID_NAME\n", error);

    SetLastError(0xdeadbeef);
    ret = pIsDomainLegalCookieDomainW(dot_com, com);
    error = GetLastError();
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");
    ok(error == ERROR_INVALID_NAME ||
        broken(error == 0xdeadbeef), /* IE6 */
        "got %u expected ERROR_INVALID_NAME\n", error);

    SetLastError(0xdeadbeef);
    ret = pIsDomainLegalCookieDomainW(com, gmail_com);
    error = GetLastError();
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");
    ok(error == ERROR_SXS_KEY_NOT_FOUND ||
        error == ERROR_SUCCESS || /* IE8 on W2K3 */
        error == 0xdeadbeef, /* up to IE7 */
        "unexpected error: %u\n", error);

    ret = pIsDomainLegalCookieDomainW(gmail_com, gmail_com);
    ok(ret, "IsDomainLegalCookieDomainW failed\n");

    SetLastError(0xdeadbeef);
    ret = pIsDomainLegalCookieDomainW(gmail_co_uk, co_uk);
    error = GetLastError();
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");
    ok(error == ERROR_SXS_KEY_NOT_FOUND || /* IE8 on XP */
        error == ERROR_FILE_NOT_FOUND ||   /* IE8 on Vista */
        error == ERROR_SUCCESS || /* IE8 on W2K3 */
        error == 0xdeadbeef, /* up to IE7 */
        "unexpected error: %u\n", error);

    ret = pIsDomainLegalCookieDomainW(uk, co_uk);
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");

    ret = pIsDomainLegalCookieDomainW(gmail_co_uk, dot_co_uk);
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");

    ret = pIsDomainLegalCookieDomainW(gmail_co_uk, gmail_co_uk);
    ok(ret, "IsDomainLegalCookieDomainW failed\n");

    ret = pIsDomainLegalCookieDomainW(gmail_com, com);
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");

    SetLastError(0xdeadbeef);
    ret = pIsDomainLegalCookieDomainW(dot_gmail_com, mail_gmail_com);
    error = GetLastError();
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");
    ok(error == ERROR_INVALID_NAME ||
        broken(error == 0xdeadbeef), /* IE6 */
        "got %u expected ERROR_INVALID_NAME\n", error);

    ret = pIsDomainLegalCookieDomainW(gmail_com, mail_gmail_com);
    ok(ret, "IsDomainLegalCookieDomainW failed\n");

    ret = pIsDomainLegalCookieDomainW(mail_gmail_com, gmail_com);
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");

    ret = pIsDomainLegalCookieDomainW(mail_gmail_com, com);
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");

    ret = pIsDomainLegalCookieDomainW(dot_gmail_com, mail_gmail_com);
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");

    ret = pIsDomainLegalCookieDomainW(mail_gmail_com, dot_gmail_com);
    ok(!ret, "IsDomainLegalCookieDomainW succeeded\n");
}

static void test_PrivacyGetSetZonePreferenceW(void)
{
    DWORD ret, zone, type, template, old_template;

    zone = 3;
    type = 0;
    ret = pPrivacyGetZonePreferenceW(zone, type, NULL, NULL, NULL);
    ok(ret == 0, "expected ret == 0, got %u\n", ret);

    old_template = 0;
    ret = pPrivacyGetZonePreferenceW(zone, type, &old_template, NULL, NULL);
    ok(ret == 0, "expected ret == 0, got %u\n", ret);

    template = 5;
    ret = pPrivacySetZonePreferenceW(zone, type, template, NULL);
    ok(ret == 0, "expected ret == 0, got %u\n", ret);

    template = 0;
    ret = pPrivacyGetZonePreferenceW(zone, type, &template, NULL, NULL);
    ok(ret == 0, "expected ret == 0, got %u\n", ret);
    ok(template == 5, "expected template == 5, got %u\n", template);

    template = 5;
    ret = pPrivacySetZonePreferenceW(zone, type, old_template, NULL);
    ok(ret == 0, "expected ret == 0, got %u\n", ret);
}

static void test_InternetSetOption(void)
{
    HINTERNET ses, con, req;
    ULONG ulArg;
    DWORD size;
    BOOL ret;

    ses = InternetOpen(NULL, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    ok(ses != 0, "InternetOpen failed: 0x%08x\n", GetLastError());
    con = InternetConnect(ses, "www.winehq.org", 80, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    ok(con != 0, "InternetConnect failed: 0x%08x\n", GetLastError());
    req = HttpOpenRequest(con, "GET", "/", NULL, NULL, NULL, 0, 0);
    ok(req != 0, "HttpOpenRequest failed: 0x%08x\n", GetLastError());

    /* INTERNET_OPTION_POLICY tests */
    SetLastError(0xdeadbeef);
    ret = InternetSetOptionW(ses, INTERNET_OPTION_POLICY, NULL, 0);
    ok(ret == FALSE, "InternetSetOption should've failed\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "GetLastError should've "
            "given ERROR_INVALID_PARAMETER, gave: 0x%08x\n", GetLastError());

    SetLastError(0xdeadbeef);
    ret = InternetQueryOptionW(ses, INTERNET_OPTION_POLICY, NULL, 0);
    ok(ret == FALSE, "InternetQueryOption should've failed\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "GetLastError should've "
            "given ERROR_INVALID_PARAMETER, gave: 0x%08x\n", GetLastError());

    /* INTERNET_OPTION_ERROR_MASK tests */
    SetLastError(0xdeadbeef);
    size = sizeof(ulArg);
    ret = InternetQueryOptionW(NULL, INTERNET_OPTION_ERROR_MASK, (void*)&ulArg, &size);
    ok(ret == FALSE, "InternetQueryOption should've failed\n");
    ok(GetLastError() == ERROR_INTERNET_INCORRECT_HANDLE_TYPE, "GetLastError() = %x\n", GetLastError());

    SetLastError(0xdeadbeef);
    ulArg = 11;
    ret = InternetSetOption(NULL, INTERNET_OPTION_ERROR_MASK, (void*)&ulArg, sizeof(ULONG));
    ok(ret == FALSE, "InternetQueryOption should've failed\n");
    ok(GetLastError() == ERROR_INTERNET_INCORRECT_HANDLE_TYPE, "GetLastError() = %x\n", GetLastError());

    SetLastError(0xdeadbeef);
    ulArg = 11;
    ret = InternetSetOption(req, INTERNET_OPTION_ERROR_MASK, (void*)&ulArg, 20);
    ok(ret == FALSE, "InternetQueryOption should've failed\n");
    ok(GetLastError() == ERROR_INTERNET_BAD_OPTION_LENGTH, "GetLastError() = %d\n", GetLastError());

    SetLastError(0xdeadbeef);
    ulArg = 11;
    ret = InternetSetOption(req, INTERNET_OPTION_ERROR_MASK, (void*)&ulArg, sizeof(ULONG));
    ok(ret == TRUE, "InternetQueryOption should've succeeded\n");
    ok(GetLastError() == 0xdeadbeef, "GetLastError() = %d\n", GetLastError());

    SetLastError(0xdeadbeef);
    ulArg = 4;
    ret = InternetSetOption(req, INTERNET_OPTION_ERROR_MASK, (void*)&ulArg, sizeof(ULONG));
    ok(ret == FALSE, "InternetQueryOption should've failed\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "GetLastError() = %x\n", GetLastError());

    SetLastError(0xdeadbeef);
    ulArg = 16;
    ret = InternetSetOption(req, INTERNET_OPTION_ERROR_MASK, (void*)&ulArg, sizeof(ULONG));
    ok(ret == FALSE, "InternetQueryOption should've failed\n");
    ok(GetLastError() == ERROR_INVALID_PARAMETER, "GetLastError() = %x\n", GetLastError());

    ret = InternetCloseHandle(req);
    ok(ret == TRUE, "InternetCloseHandle failed: 0x%08x\n", GetLastError());
    ret = InternetCloseHandle(con);
    ok(ret == TRUE, "InternetCloseHandle failed: 0x%08x\n", GetLastError());
    ret = InternetCloseHandle(ses);
    ok(ret == TRUE, "InternetCloseHandle failed: 0x%08x\n", GetLastError());
}

#define verifyProxyEnable(e) r_verifyProxyEnable(__LINE__, e)
static void r_verifyProxyEnable(LONG l, DWORD exp)
{
    HKEY hkey;
    DWORD type, val, size = sizeof(DWORD);
    LONG ret;
    static const CHAR szInternetSettings[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";
    static const CHAR szProxyEnable[] = "ProxyEnable";

    ret = RegOpenKeyA(HKEY_CURRENT_USER, szInternetSettings, &hkey);
    ok_(__FILE__,l) (!ret, "RegOpenKeyA failed: 0x%08x\n", ret);

    ret = RegQueryValueExA(hkey, szProxyEnable, 0, &type, (BYTE*)&val, &size);
    ok_(__FILE__,l) (!ret, "RegQueryValueExA failed: 0x%08x\n", ret);
    ok_(__FILE__,l) (type == REG_DWORD, "Expected regtype to be REG_DWORD, was: %d\n", type);
    ok_(__FILE__,l) (val == exp, "Expected ProxyEnabled to be %d, got: %d\n", exp, val);

    ret = RegCloseKey(hkey);
    ok_(__FILE__,l) (!ret, "RegCloseKey failed: 0x%08x\n", ret);
}

static void test_Option_PerConnectionOption(void)
{
    BOOL ret;
    DWORD size = sizeof(INTERNET_PER_CONN_OPTION_LISTW);
    INTERNET_PER_CONN_OPTION_LISTW list = {size};
    INTERNET_PER_CONN_OPTIONW *orig_settings;
    static WCHAR proxy_srvW[] = {'p','r','o','x','y','.','e','x','a','m','p','l','e',0};

    /* get the global IE proxy server info, to restore later */
    list.dwOptionCount = 2;
    list.pOptions = HeapAlloc(GetProcessHeap(), 0, 2 * sizeof(INTERNET_PER_CONN_OPTIONW));

    list.pOptions[0].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
    list.pOptions[1].dwOption = INTERNET_PER_CONN_FLAGS;

    ret = InternetQueryOptionW(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
            &list, &size);
    ok(ret == TRUE, "InternetQueryOption should've succeeded\n");
    orig_settings = list.pOptions;

    /* set the global IE proxy server */
    list.dwOptionCount = 2;
    list.pOptions = HeapAlloc(GetProcessHeap(), 0, 2 * sizeof(INTERNET_PER_CONN_OPTIONW));

    list.pOptions[0].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
    list.pOptions[0].Value.pszValue = proxy_srvW;
    list.pOptions[1].dwOption = INTERNET_PER_CONN_FLAGS;
    list.pOptions[1].Value.dwValue = PROXY_TYPE_PROXY;

    ret = InternetSetOptionW(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
            &list, size);
    ok(ret == TRUE, "InternetSetOption should've succeeded\n");

    HeapFree(GetProcessHeap(), 0, list.pOptions);

    /* get & verify the global IE proxy server */
    list.dwOptionCount = 2;
    list.dwOptionError = 0;
    list.pOptions = HeapAlloc(GetProcessHeap(), 0, 2 * sizeof(INTERNET_PER_CONN_OPTIONW));

    list.pOptions[0].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
    list.pOptions[1].dwOption = INTERNET_PER_CONN_FLAGS;

    ret = InternetQueryOptionW(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
            &list, &size);
    ok(ret == TRUE, "InternetQueryOption should've succeeded\n");
    ok(!strcmp_ww(list.pOptions[0].Value.pszValue, proxy_srvW),
            "Retrieved proxy server should've been %s, was: %s\n",
            wine_dbgstr_w(proxy_srvW), wine_dbgstr_w(list.pOptions[0].Value.pszValue));
    ok(list.pOptions[1].Value.dwValue == PROXY_TYPE_PROXY,
            "Retrieved flags should've been PROXY_TYPE_PROXY, was: %d\n",
            list.pOptions[1].Value.dwValue);
    verifyProxyEnable(1);

    HeapFree(GetProcessHeap(), 0, list.pOptions[0].Value.pszValue);
    HeapFree(GetProcessHeap(), 0, list.pOptions);

    /* disable the proxy server */
    list.dwOptionCount = 1;
    list.pOptions = HeapAlloc(GetProcessHeap(), 0, sizeof(INTERNET_PER_CONN_OPTIONW));

    list.pOptions[0].dwOption = INTERNET_PER_CONN_FLAGS;
    list.pOptions[0].Value.dwValue = PROXY_TYPE_DIRECT;

    ret = InternetSetOptionW(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
            &list, size);
    ok(ret == TRUE, "InternetSetOption should've succeeded\n");

    HeapFree(GetProcessHeap(), 0, list.pOptions);

    /* verify that the proxy is disabled */
    list.dwOptionCount = 1;
    list.dwOptionError = 0;
    list.pOptions = HeapAlloc(GetProcessHeap(), 0, sizeof(INTERNET_PER_CONN_OPTIONW));

    list.pOptions[0].dwOption = INTERNET_PER_CONN_FLAGS;

    ret = InternetQueryOptionW(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
            &list, &size);
    ok(ret == TRUE, "InternetQueryOption should've succeeded\n");
    ok(list.pOptions[0].Value.dwValue == PROXY_TYPE_DIRECT,
            "Retrieved flags should've been PROXY_TYPE_DIRECT, was: %d\n",
            list.pOptions[0].Value.dwValue);
    verifyProxyEnable(0);

    HeapFree(GetProcessHeap(), 0, list.pOptions);

    /* set the proxy flags to 'invalid' value */
    list.dwOptionCount = 1;
    list.pOptions = HeapAlloc(GetProcessHeap(), 0, sizeof(INTERNET_PER_CONN_OPTIONW));

    list.pOptions[0].dwOption = INTERNET_PER_CONN_FLAGS;
    list.pOptions[0].Value.dwValue = PROXY_TYPE_PROXY | PROXY_TYPE_DIRECT;

    ret = InternetSetOptionW(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
            &list, size);
    ok(ret == TRUE, "InternetSetOption should've succeeded\n");

    HeapFree(GetProcessHeap(), 0, list.pOptions);

    /* verify that the proxy is enabled */
    list.dwOptionCount = 1;
    list.dwOptionError = 0;
    list.pOptions = HeapAlloc(GetProcessHeap(), 0, sizeof(INTERNET_PER_CONN_OPTIONW));

    list.pOptions[0].dwOption = INTERNET_PER_CONN_FLAGS;

    ret = InternetQueryOptionW(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
            &list, &size);
    ok(ret == TRUE, "InternetQueryOption should've succeeded\n");
    todo_wine ok(list.pOptions[0].Value.dwValue == (PROXY_TYPE_PROXY | PROXY_TYPE_DIRECT),
            "Retrieved flags should've been PROXY_TYPE_PROXY | PROXY_TYPE_DIRECT, was: %d\n",
            list.pOptions[0].Value.dwValue);
    verifyProxyEnable(1);

    HeapFree(GetProcessHeap(), 0, list.pOptions);

    /* restore original settings */
    list.dwOptionCount = 2;
    list.pOptions = orig_settings;

    ret = InternetSetOptionW(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
            &list, size);
    ok(ret == TRUE, "InternetSetOption should've succeeded\n");

    HeapFree(GetProcessHeap(), 0, list.pOptions);
}

static void test_Option_PerConnectionOptionA(void)
{
    BOOL ret;
    DWORD size = sizeof(INTERNET_PER_CONN_OPTION_LISTA);
    INTERNET_PER_CONN_OPTION_LISTA list = {size};
    INTERNET_PER_CONN_OPTIONA *orig_settings;
    char proxy_srv[] = "proxy.example";

    /* get the global IE proxy server info, to restore later */
    list.dwOptionCount = 2;
    list.pOptions = HeapAlloc(GetProcessHeap(), 0, 2 * sizeof(INTERNET_PER_CONN_OPTIONA));

    list.pOptions[0].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
    list.pOptions[1].dwOption = INTERNET_PER_CONN_FLAGS;

    ret = InternetQueryOptionA(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
            &list, &size);
    ok(ret == TRUE, "InternetQueryOption should've succeeded\n");
    orig_settings = list.pOptions;

    /* set the global IE proxy server */
    list.dwOptionCount = 2;
    list.pOptions = HeapAlloc(GetProcessHeap(), 0, 2 * sizeof(INTERNET_PER_CONN_OPTIONA));

    list.pOptions[0].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
    list.pOptions[0].Value.pszValue = proxy_srv;
    list.pOptions[1].dwOption = INTERNET_PER_CONN_FLAGS;
    list.pOptions[1].Value.dwValue = PROXY_TYPE_PROXY;

    ret = InternetSetOptionA(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
            &list, size);
    ok(ret == TRUE, "InternetSetOption should've succeeded\n");

    HeapFree(GetProcessHeap(), 0, list.pOptions);

    /* get & verify the global IE proxy server */
    list.dwOptionCount = 2;
    list.dwOptionError = 0;
    list.pOptions = HeapAlloc(GetProcessHeap(), 0, 2 * sizeof(INTERNET_PER_CONN_OPTIONA));

    list.pOptions[0].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
    list.pOptions[1].dwOption = INTERNET_PER_CONN_FLAGS;

    ret = InternetQueryOptionA(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
            &list, &size);
    ok(ret == TRUE, "InternetQueryOption should've succeeded\n");
    ok(!lstrcmpA(list.pOptions[0].Value.pszValue, "proxy.example"),
            "Retrieved proxy server should've been \"%s\", was: \"%s\"\n",
            proxy_srv, list.pOptions[0].Value.pszValue);
    ok(list.pOptions[1].Value.dwValue == PROXY_TYPE_PROXY,
            "Retrieved flags should've been PROXY_TYPE_PROXY, was: %d\n",
            list.pOptions[1].Value.dwValue);

    HeapFree(GetProcessHeap(), 0, list.pOptions[0].Value.pszValue);
    HeapFree(GetProcessHeap(), 0, list.pOptions);

    /* restore original settings */
    list.dwOptionCount = 2;
    list.pOptions = orig_settings;

    ret = InternetSetOptionA(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION,
            &list, size);
    ok(ret == TRUE, "InternetSetOption should've succeeded\n");

    HeapFree(GetProcessHeap(), 0, list.pOptions);
}

#define FLAG_TODO     0x1
#define FLAG_NEEDREQ  0x2
#define FLAG_UNIMPL   0x4

void test_InternetErrorDlg(void)
{
    HINTERNET ses, con, req;
    DWORD res, flags;
    HWND hwnd;
    ULONG i;
    static const struct {
        DWORD error;
        DWORD res;
        DWORD test_flags;
    } no_ui_res[] = {
        { ERROR_INTERNET_INCORRECT_PASSWORD     , ERROR_SUCCESS, FLAG_NEEDREQ },
        { ERROR_INTERNET_SEC_CERT_DATE_INVALID  , ERROR_CANCELLED, 0 },
        { ERROR_INTERNET_SEC_CERT_CN_INVALID    , ERROR_CANCELLED, 0 },
        { ERROR_INTERNET_HTTP_TO_HTTPS_ON_REDIR , ERROR_SUCCESS, 0 },
        { ERROR_INTERNET_HTTPS_TO_HTTP_ON_REDIR , ERROR_SUCCESS, FLAG_TODO },
        { ERROR_INTERNET_MIXED_SECURITY         , ERROR_CANCELLED, FLAG_TODO },
        { ERROR_INTERNET_CHG_POST_IS_NON_SECURE , ERROR_CANCELLED, FLAG_TODO },
        { ERROR_INTERNET_POST_IS_NON_SECURE     , ERROR_SUCCESS, 0 },
        { ERROR_INTERNET_CLIENT_AUTH_CERT_NEEDED, ERROR_CANCELLED, FLAG_NEEDREQ|FLAG_TODO },
        { ERROR_INTERNET_INVALID_CA             , ERROR_CANCELLED, 0 },
        { ERROR_INTERNET_HTTPS_HTTP_SUBMIT_REDIR, ERROR_CANCELLED, FLAG_TODO },
        { ERROR_INTERNET_INSERT_CDROM           , ERROR_CANCELLED, FLAG_TODO|FLAG_NEEDREQ|FLAG_UNIMPL },
        { ERROR_INTERNET_SEC_CERT_ERRORS        , ERROR_CANCELLED, 0 },
        { ERROR_INTERNET_SEC_CERT_REV_FAILED    , ERROR_CANCELLED, FLAG_TODO },
        { ERROR_HTTP_COOKIE_NEEDS_CONFIRMATION  , ERROR_HTTP_COOKIE_DECLINED, FLAG_TODO },
        { ERROR_INTERNET_BAD_AUTO_PROXY_SCRIPT  , ERROR_CANCELLED, FLAG_TODO },
        { ERROR_INTERNET_UNABLE_TO_DOWNLOAD_SCRIPT, ERROR_CANCELLED, FLAG_TODO },
        { ERROR_HTTP_REDIRECT_NEEDS_CONFIRMATION, ERROR_CANCELLED, FLAG_TODO },
        { ERROR_INTERNET_SEC_CERT_REVOKED       , ERROR_CANCELLED, 0 },
        { 0, ERROR_NOT_SUPPORTED }
    };

    flags = 0;

    res = InternetErrorDlg(NULL, NULL, 12055, flags, NULL);
    ok(res == ERROR_INVALID_HANDLE, "Got %d\n", res);

    ses = InternetOpen(NULL, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    ok(ses != 0, "InternetOpen failed: 0x%08x\n", GetLastError());
    con = InternetConnect(ses, "www.winehq.org", 80, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    ok(con != 0, "InternetConnect failed: 0x%08x\n", GetLastError());
    req = HttpOpenRequest(con, "GET", "/", NULL, NULL, NULL, 0, 0);
    ok(req != 0, "HttpOpenRequest failed: 0x%08x\n", GetLastError());

    /* NULL hwnd and FLAGS_ERROR_UI_FLAGS_NO_UI not set */
    for(i = INTERNET_ERROR_BASE; i < INTERNET_ERROR_LAST; i++)
    {
        res = InternetErrorDlg(NULL, req, i, flags, NULL);
        ok(res == ERROR_INVALID_HANDLE, "Got %d (%d)\n", res, i);
    }

    hwnd = GetDesktopWindow();
    ok(hwnd != NULL, "GetDesktopWindow failed (%d)\n", GetLastError());

    flags = FLAGS_ERROR_UI_FLAGS_NO_UI;
    for(i = INTERNET_ERROR_BASE; i < INTERNET_ERROR_LAST; i++)
    {
        DWORD expected, test_flags, j;

        for(j = 0; no_ui_res[j].error != 0; ++j)
            if(no_ui_res[j].error == i)
                break;

        test_flags = no_ui_res[j].test_flags;
        expected = no_ui_res[j].res;

        /* Try an invalid request handle */
        res = InternetErrorDlg(hwnd, (HANDLE)0xdeadbeef, i, flags, NULL);
        if(res == ERROR_CALL_NOT_IMPLEMENTED)
        {
            todo_wine ok(test_flags & FLAG_UNIMPL, "%i is unexpectedly unimplemented.\n", i);
            continue;
        }
        else
            todo_wine ok(res == ERROR_INVALID_HANDLE, "Got %d (%d)\n", res, i);

        /* With a valid req */
        if(i == ERROR_INTERNET_NEED_UI)
            continue; /* Crashes on windows XP */

        if(i == ERROR_INTERNET_SEC_CERT_REVOKED)
            continue; /* Interactive (XP, Win7) */

        res = InternetErrorDlg(hwnd, req, i, flags, NULL);

        /* Handle some special cases */
        switch(i)
        {
        case ERROR_INTERNET_HTTP_TO_HTTPS_ON_REDIR:
        case ERROR_INTERNET_HTTPS_TO_HTTP_ON_REDIR:
            if(res == ERROR_CANCELLED)
            {
                /* Some windows XP, w2k3 x64, W2K8 */
                win_skip("Skipping some tests for %d\n", i);
                continue;
            }
            break;
        case ERROR_INTERNET_FORTEZZA_LOGIN_NEEDED:
            if(res != expected)
            {
                /* Windows XP, W2K3 */
                ok(res == NTE_PROV_TYPE_NOT_DEF, "Got %d\n", res);
                win_skip("Skipping some tests for %d\n", i);
                continue;
            }
            break;
        default: break;
        }

        if(test_flags & FLAG_TODO)
            todo_wine ok(res == expected, "Got %d, expected %d (%d)\n", res, expected, i);
        else
            ok(res == expected, "Got %d, expected %d (%d)\n", res, expected, i);

        /* Same thing with NULL hwnd */
        res = InternetErrorDlg(NULL, req, i, flags, NULL);
        if(test_flags & FLAG_TODO)
            todo_wine ok(res == expected, "Got %d, expected %d (%d)\n", res, expected, i);
        else
            ok(res == expected, "Got %d, expected %d (%d)\n", res, expected, i);


        /* With a null req */
        if(test_flags & FLAG_NEEDREQ)
            expected = ERROR_INVALID_PARAMETER;

        res = InternetErrorDlg(hwnd, NULL, i, flags, NULL);
        if( test_flags & FLAG_TODO || i == ERROR_INTERNET_INCORRECT_PASSWORD)
            todo_wine ok(res == expected, "Got %d, expected %d (%d)\n", res, expected, i);
        else
            ok(res == expected, "Got %d, expected %d (%d)\n", res, expected, i);
    }

    res = InternetCloseHandle(req);
    ok(res == TRUE, "InternetCloseHandle failed: 0x%08x\n", GetLastError());
    res = InternetCloseHandle(con);
    ok(res == TRUE, "InternetCloseHandle failed: 0x%08x\n", GetLastError());
    res = InternetCloseHandle(ses);
    ok(res == TRUE, "InternetCloseHandle failed: 0x%08x\n", GetLastError());
}

/* ############################### */

START_TEST(internet)
{
    HMODULE hdll;
    hdll = GetModuleHandleA("wininet.dll");

    if(!GetProcAddress(hdll, "InternetGetCookieExW")) {
        win_skip("Too old IE (older than 6.0)\n");
        return;
    }

    pCreateUrlCacheContainerA = (void*)GetProcAddress(hdll, "CreateUrlCacheContainerA");
    pCreateUrlCacheContainerW = (void*)GetProcAddress(hdll, "CreateUrlCacheContainerW");
    pInternetTimeFromSystemTimeA = (void*)GetProcAddress(hdll, "InternetTimeFromSystemTimeA");
    pInternetTimeFromSystemTimeW = (void*)GetProcAddress(hdll, "InternetTimeFromSystemTimeW");
    pInternetTimeToSystemTimeA = (void*)GetProcAddress(hdll, "InternetTimeToSystemTimeA");
    pInternetTimeToSystemTimeW = (void*)GetProcAddress(hdll, "InternetTimeToSystemTimeW");
    pIsDomainLegalCookieDomainW = (void*)GetProcAddress(hdll, (LPCSTR)117);
    pPrivacyGetZonePreferenceW = (void*)GetProcAddress(hdll, "PrivacyGetZonePreferenceW");
    pPrivacySetZonePreferenceW = (void*)GetProcAddress(hdll, "PrivacySetZonePreferenceW");

    test_InternetCanonicalizeUrlA();
    test_InternetQueryOptionA();
    test_get_cookie();
    test_complicated_cookie();
    test_version();
    test_null();
    test_Option_PerConnectionOption();
    test_Option_PerConnectionOptionA();
    test_InternetErrorDlg();

    if (!pInternetTimeFromSystemTimeA)
        win_skip("skipping the InternetTime tests\n");
    else
    {
        InternetTimeFromSystemTimeA_test();
        InternetTimeFromSystemTimeW_test();
        InternetTimeToSystemTimeA_test();
        InternetTimeToSystemTimeW_test();
    }
    if (pIsDomainLegalCookieDomainW &&
        ((void*)pIsDomainLegalCookieDomainW == (void*)pCreateUrlCacheContainerA ||
         (void*)pIsDomainLegalCookieDomainW == (void*)pCreateUrlCacheContainerW))
        win_skip("IsDomainLegalCookieDomainW is not available on systems with IE5\n");
    else if (!pIsDomainLegalCookieDomainW)
        win_skip("IsDomainLegalCookieDomainW (or ordinal 117) is not available\n");
    else
        test_IsDomainLegalCookieDomainW();

    if (pPrivacyGetZonePreferenceW && pPrivacySetZonePreferenceW)
        test_PrivacyGetSetZonePreferenceW();
    else
        win_skip("Privacy[SG]etZonePreferenceW are not available\n");

    test_InternetSetOption();
}
