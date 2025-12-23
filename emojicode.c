#include <windows.h>
#include <stdio.h>
#include <string.h>

// hex to emoji mapping where each emoji is a surrogate pair
LPCWSTR hex_to_emoji[16] = {
    L"\U0001F600", // 0 😀
    L"\U0001F601", // 1 😁
    L"\U0001F602", // 2 😂
    L"\U0001F603", // 3 😃
    L"\U0001F604", // 4 😄
    L"\U0001F605", // 5 😅
    L"\U0001F606", // 6 😆
    L"\U0001F609", // 7 😉
    L"\U0001F60A", // 8 😊
    L"\U0001F60B", // 9 😋
    L"\U0001F60E", // A 😎
    L"\U0001F60D", // B 😍
    L"\U0001F618", // C 😘
    L"\U0001F617", // D 😗
    L"\U0001F619", // E 😙
    L"\U0001F61A"  // F 😚
};

BOOL BytesToEmoji(IN PBYTE buffer, IN SIZE_T size, OUT LPSTR* outUtf8)
{
    SIZE_T estLen, windex;
    LPWSTR wstr;
    LPSTR utf8;
    BYTE b, high, low;
    INT utf8Len;

    if (!buffer || !outUtf8 || size == 0) {
        printf("[!] Invalid input to BytesToEmoji\n");
        return FALSE;
    }

    *outUtf8 = NULL;

    // allocate wide char buffer (2 emojis per byte, 2 WCHAR per emoji)
    estLen = size * 2 * 2 + 1;
    wstr = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, estLen * sizeof(WCHAR));
    if (!wstr) {
        printf("[!] HeapAlloc failed\n");
        return FALSE;
    }

    windex = 0;
    for (SIZE_T i = 0; i < size; i++) {
        b = buffer[i];
        high = (b >> 4) & 0xF;
        low = b & 0xF;

        // copy full surrogate pair
        wstr[windex++] = hex_to_emoji[high][0];
        wstr[windex++] = hex_to_emoji[high][1];
        wstr[windex++] = hex_to_emoji[low][0];
        wstr[windex++] = hex_to_emoji[low][1];
    }
    wstr[windex] = L'\0';

    // convert wide char to UTF-8
    utf8Len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    if (utf8Len <= 0) {
        printf("[!] WideCharToMultiByte failed\n");
        HeapFree(GetProcessHeap(), 0, wstr);
        return FALSE;
    }

    utf8 = (LPSTR)HeapAlloc(GetProcessHeap(), 0, utf8Len);
    if (!utf8) {
        printf("[!] HeapAlloc failed\n");
        HeapFree(GetProcessHeap(), 0, wstr);
        return FALSE;
    }

    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8, utf8Len, NULL, NULL);
    HeapFree(GetProcessHeap(), 0, wstr);

    *outUtf8 = utf8;
    return TRUE;
}

BOOL EmojiToBytes(IN LPCSTR emojiUtf8, OUT PBYTE* outBytes, OUT PSIZE_T outSize)
{

    INT lenW;
    LPWSTR wbuffer;
    PBYTE buffer;
    BYTE currentByte;
    SIZE_T bufSize, bindex;
    BOOL highNibble;

    // check if valid usage
    if (!emojiUtf8 || !outBytes || !outSize) {
        printf("[!] Invalid input to EmojiToBytes\n");
        return FALSE;
    }

    *outBytes = NULL;
    *outSize = 0;

    // convert utf-8 to wide characters
    lenW = MultiByteToWideChar(CP_UTF8, 0, emojiUtf8, -1, NULL, 0);
    if (lenW <= 0) {
        printf("[!] MultiByteToWideChar failed with error: %d\n", GetLastError());
        return FALSE;
    }

    wbuffer = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, lenW * sizeof(WCHAR));
    if (!wbuffer) {
        printf("[!] HeapAlloc failed\n");
        return FALSE;
    }

    MultiByteToWideChar(CP_UTF8, 0, emojiUtf8, -1, wbuffer, lenW);

    // allocate buffer for bytes
    bufSize = lenW / 4 + 1;
    buffer = (PBYTE)HeapAlloc(GetProcessHeap(), 0, bufSize);
    if (!buffer) {
        printf("[!] HeapAlloc failed\n");
        HeapFree(GetProcessHeap(), 0, wbuffer);
        return FALSE;
    }

    bindex = 0;
    currentByte = 0;
    highNibble = TRUE;

    for (int i = 0; i + 1 < lenW; i += 2) {
        int hexVal = -1;

        // match surrogate pair
        for (int j = 0; j < 16; j++) {
            if (wbuffer[i] == hex_to_emoji[j][0] &&
                wbuffer[i + 1] == hex_to_emoji[j][1]) {
                hexVal = j;
                break;
            }
        }

        if (hexVal == -1) continue;

        if (highNibble) {
            currentByte = (BYTE)(hexVal << 4);
            highNibble = FALSE;
        }
        else {
            currentByte |= (BYTE)hexVal;
            buffer[bindex++] = currentByte;
            currentByte = 0;
            highNibble = TRUE;
        }
    }

    HeapFree(GetProcessHeap(), 0, wbuffer);

    *outBytes = buffer;
    *outSize = bindex;
    return TRUE;
}

BOOL BinFileToEmojiText(IN LPCWSTR binFilename, IN LPCWSTR textFilename)
{
    HANDLE hFile = NULL;
    DWORD sizeLow = 0, bytesRead = 0, written, len;
    PBYTE buffer = NULL;
    LPSTR emojiString = NULL;

    // check input
    if (!binFilename || !textFilename) {
        printf("[!] Invalid usage of BinFileToEmojiText\n");
        return FALSE;
    }

    hFile = CreateFileW(binFilename, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("[!] CreateFileW failed with error: %d\n", GetLastError());
        return FALSE;
    }

    sizeLow = GetFileSize(hFile, NULL);
    if (sizeLow == INVALID_FILE_SIZE) {
        printf("[!] GetFileSize failed with error: %d\n", GetLastError());
        CloseHandle(hFile);
        return FALSE;
    }

    buffer = (PBYTE)HeapAlloc(GetProcessHeap(), 0, sizeLow);
    if (!buffer) {
        printf("[!] HeapAlloc failed with error: %d\n", GetLastError());
        CloseHandle(hFile);
        return FALSE;
    }

    if (!ReadFile(hFile, buffer, sizeLow, &bytesRead, NULL) || bytesRead != sizeLow) {
        printf("[!] ReadFile failed with error: %d\n", GetLastError());
        HeapFree(GetProcessHeap(), 0, buffer);
        CloseHandle(hFile);
        return FALSE;
    }

    CloseHandle(hFile);

    if (!BytesToEmoji(buffer, sizeLow, &emojiString)) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return FALSE;
    }

    HeapFree(GetProcessHeap(), 0, buffer);

    hFile = CreateFileW(textFilename, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
        printf("[!] CreateFileW failed with error: %d\n", GetLastError());
        HeapFree(GetProcessHeap(), 0, emojiString);
        return FALSE;
    }

    written = 0;
    len = (DWORD)strlen(emojiString);
    if (!WriteFile(hFile, emojiString, len, &written, NULL)) {
        printf("[!] WriteFile failed with error: %d\n", GetLastError());
        HeapFree(GetProcessHeap(), 0, emojiString);
        return FALSE;
    }

    HeapFree(GetProcessHeap(), 0, emojiString);
    CloseHandle(hFile);
    return TRUE;
}

BOOL EmojiTextToBinFile(IN LPCWSTR textFilename, IN LPCWSTR binFilename)
{
    HANDLE hFile = NULL;
    DWORD sizeLow = 0, bytesRead = 0, written;
    LPSTR textBuffer = NULL;
    PBYTE byteBuffer = NULL;
    SIZE_T byteSize = 0;
 
    if (!textFilename || !binFilename) {
        printf("[!] Invalid usage of EmojiTextToBinFile\n");
        return FALSE;
    }

    hFile = CreateFileW(textFilename, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
        printf("[!] CreateFileW failed with error: %d\n", GetLastError());
        return FALSE;
    }

    sizeLow = GetFileSize(hFile, NULL);
    textBuffer = (LPSTR)HeapAlloc(GetProcessHeap(), 0, sizeLow + 1);
    if (!textBuffer) {
        printf("[!] HeapAlloc failed with error: %d\n", GetLastError());
        CloseHandle(hFile);
        return FALSE;
    }

    if (!ReadFile(hFile, textBuffer, sizeLow, &bytesRead, NULL)) {
        printf("[!] ReadFile failed with error: %d\n", GetLastError());
        return FALSE;
    }

    textBuffer[sizeLow] = '\0';
    if (!CloseHandle(hFile)) {
        printf("[!] CloseHandle failed with error: %d\n", GetLastError());
    }

    if (!EmojiToBytes(textBuffer, &byteBuffer, &byteSize)) {
        HeapFree(GetProcessHeap(), 0, textBuffer);
        return FALSE;
    }

    HeapFree(GetProcessHeap(), 0, textBuffer);

    hFile = CreateFileW(binFilename, GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == NULL || hFile == INVALID_HANDLE_VALUE) {
        printf("[!] CreateFileW failed with error: %d\n", GetLastError());
        HeapFree(GetProcessHeap(), 0, byteBuffer);
        return FALSE;
    }

    written = 0;
    WriteFile(hFile, byteBuffer, (DWORD)byteSize, &written, NULL);

    HeapFree(GetProcessHeap(), 0, byteBuffer);
    CloseHandle(hFile);
    return TRUE;
}

INT GetFileTypeByExtension(IN LPCWSTR filename)
{
    if (!filename) return 0;

    SIZE_T len = wcslen(filename);
    if (len < 4) return 0;

    LPCWSTR ext = filename + len - 4;
    if (_wcsicmp(ext, L".bin") == 0) return 1;
    if (_wcsicmp(ext, L".txt") == 0) return 2;

    printf("[!] invalid filetype. please use .txt or .bin\n");
    return 0;
}


// main function
int wmain(int argc, wchar_t* argv[])
{
    SetConsoleOutputCP(CP_UTF8);

    if (argc != 3) {
        wprintf(L"[!] Usage: %s <input file> <output file>\n", argv[0]);
        return -1;
    }

    int type = GetFileTypeByExtension(argv[1]);

    if (type == 1) {
        if (!BinFileToEmojiText(argv[1], argv[2])) return -1;
    }
    else if (type == 2) {
        if (!EmojiTextToBinFile(argv[1], argv[2])) return -1;
    }
    else {
        printf("[!] Invalid input file type\n");
        return -1;
    }

    printf("[+] Conversion successful\n");
    return 0;
}
