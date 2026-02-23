#include <iostream>
#include <fstream>
#include <string>
#include "Windows.h"
#include <sstream>
#include <vector>

using namespace std;

string PIN_DIR = "";
string PINTOOL_DIR = "";
string PINEXE;
string PINTOOL64;
string CONFIG = "UnSafengine.cfg";

static string trim_copy(string s) {
	while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) {
		s.pop_back();
	}
	size_t start = 0;
	while (start < s.size() && (s[start] == ' ' || s[start] == '\t')) {
		start++;
	}
	return s.substr(start);
}

static bool file_exists(const string& path) {
	DWORD attrs = GetFileAttributesA(path.c_str());
	return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static bool dir_exists(const string& path) {
	DWORD attrs = GetFileAttributesA(path.c_str());
	return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static string join_path(const string& a, const string& b) {
	if (a.empty()) return b;
	if (a.back() == '\\' || a.back() == '/') return a + b;
	return a + "\\" + b;
}

static string quote_arg(const string& s) {
	if (s.find(' ') != string::npos || s.find('\t') != string::npos) {
		return string("\"") + s + "\"";
	}
	return s;
}

enum class pe_arch { unknown, x86, x64 };

static pe_arch get_pe_arch(const string& path) {
	ifstream in(path, ios::binary);
	if (!in) return pe_arch::unknown;
	IMAGE_DOS_HEADER dos{};
	in.read(reinterpret_cast<char*>(&dos), sizeof(dos));
	if (!in || dos.e_magic != IMAGE_DOS_SIGNATURE) return pe_arch::unknown;
	in.seekg(dos.e_lfanew, ios::beg);
	DWORD sig = 0;
	in.read(reinterpret_cast<char*>(&sig), sizeof(sig));
	if (!in || sig != IMAGE_NT_SIGNATURE) return pe_arch::unknown;
	IMAGE_FILE_HEADER fh{};
	in.read(reinterpret_cast<char*>(&fh), sizeof(fh));
	if (!in) return pe_arch::unknown;
	if (fh.Machine == IMAGE_FILE_MACHINE_I386) return pe_arch::x86;
	if (fh.Machine == IMAGE_FILE_MACHINE_AMD64) return pe_arch::x64;
	return pe_arch::unknown;
}

static string find_workspace_root_from(const string& startDir) {
	string current = startDir;
	for (int i = 0; i < 8; i++) {
		if (dir_exists(join_path(current, "pin"))) {
			return current;
		}
		size_t pos = current.find_last_of("\\/");
		if (pos == string::npos) break;
		current = current.substr(0, pos);
		if (current.size() <= 3) break; // e.g. C:\\ (root)
	}
	return startDir;
}

string get_exe_path() {
	wchar_t buffer[MAX_PATH];
	GetModuleFileName(NULL, (LPWSTR)buffer, MAX_PATH);
	wstring ws(buffer);
	string current_directory(ws.begin(), ws.end());
	string::size_type pos = current_directory.find_last_of("\\/");
	return current_directory.substr(0, pos);
}

string get_working_path() {
	wchar_t buffer[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, buffer);
	wstring ws(buffer);
	string working_directory(ws.begin(), ws.end());
	return working_directory;
}

static void set_defaults_from_workspace(const string& workspaceRoot, pe_arch arch) {
	const string pinRoot = join_path(workspaceRoot, "pin");
	const string pintoolRoot = join_path(workspaceRoot, "pintool");

	string pinExeCandidate;
	if (arch == pe_arch::x64) {
		pinExeCandidate = join_path(join_path(join_path(pinRoot, "intel64"), "bin"), "pin.exe");
	} else {
		pinExeCandidate = join_path(join_path(join_path(pinRoot, "ia32"), "bin"), "pin.exe");
	}

	PINEXE = pinExeCandidate;
	PINTOOL64 = join_path(pintoolRoot, "UnSafengine.dll");
}

int read_config_file(string config_file, pe_arch arch) {
	const string exeDir = get_exe_path();
	const string workDir = get_working_path();
	const string workspaceRoot = find_workspace_root_from(exeDir);

	vector<string> candidateConfigs{
		join_path(exeDir, config_file),
		join_path(workDir, config_file),
		join_path(workspaceRoot, config_file),
		join_path(join_path(workspaceRoot, "pintool"), config_file),
	};

	bool loaded = false;
	string configPath;
	ifstream infile;
	for (const auto& path : candidateConfigs) {
		infile.open(path);
		if (infile) {
			loaded = true;
			configPath = path;
			break;
		}
		infile.clear();
	}

	if (!loaded) {
		set_defaults_from_workspace(workspaceRoot, arch);
		return 0;
	}

	string line;
	while (std::getline(infile, line)) {
		line = trim_copy(line);
		if (line.empty()) continue;
		if (line[0] == '#') continue;

		if (line.rfind("PIN_DIR", 0) == 0) {
			size_t pos = line.find('=');
			if (pos != string::npos) {
				PIN_DIR = trim_copy(line.substr(pos + 1));
			}
		} else if (line.rfind("PINTOOL_DIR", 0) == 0) {
			size_t pos = line.find('=');
			if (pos != string::npos) {
				PINTOOL_DIR = trim_copy(line.substr(pos + 1));
			}
		}
	}

	// Build candidates from config, but only accept them if the files exist.
	if (!PIN_DIR.empty()) {
		const string pinExe = join_path(PIN_DIR, "pin.exe");
		if (file_exists(pinExe)) {
			PINEXE = pinExe;
		}
	}
	if (!PINTOOL_DIR.empty()) {
		const string pintoolDll = join_path(PINTOOL_DIR, "UnSafengine.dll");
		if (file_exists(pintoolDll)) {
			PINTOOL64 = pintoolDll;
		}
	}

	// Fallback to repo defaults if config points to non-existent locations.
	if (PINEXE.empty() || PINTOOL64.empty()) {
		set_defaults_from_workspace(workspaceRoot, arch);
	}

	return 1;
}

int check_output_file(string out_file_path) {
	if (out_file_path.substr(0, 2) == ".\\") {
		out_file_path = out_file_path.substr(2);
	}
	out_file_path = get_working_path() + "\\" + out_file_path;
	cout << out_file_path << endl;
	ifstream infile(out_file_path);
	if (!infile) {
		return 0;
	}
	return -1;
}


int main(int argc, char** argv)
{
	string option, exe_file_name;
	string cmd_line;

	if (argc < 3 || argc % 2 == 0) {
		cout << "Usage: " << endl;
		cout << "    UnSafengine.exe -deob [-log log_file_name] [-dmp dump_file_name] exe_file_name " << endl;
		cout << "    or" << endl;
		cout << "    UnSafengine.exe -trace [-log log_file_name] exe_file_name" << endl;
		cout << "    UnSafengine.exe -pauseatoep [-log log_file_name] exe_file_name" << endl;
		exit(-1);
	}

	exe_file_name = string(argv[argc - 1]);
	pe_arch arch = get_pe_arch(exe_file_name);
	read_config_file(CONFIG, arch);

	cmd_line = quote_arg(PINEXE) + " -t " + quote_arg(PINTOOL64);
	option = string(argv[1]);
	if (option == "-deob") {
		cmd_line += " -dump";
	}
	else if (option == "-trace") {
		cmd_line += " -trace";
	}
	else if (option == "-pauseatoep") {
		cmd_line += " -pauseatoep ";
	}
	else {
		cout << "incorrect option!" << endl;
		exit(1);
	}
	for (int i = 2; i < argc - 1; i += 2) {
		string opt = string(argv[i]);
		if (opt == "-log") {
			cmd_line += " -log " + quote_arg(string(argv[i + 1]));
		}
		else if (opt == "-dump") {
			cmd_line += " -dmp " + quote_arg(string(argv[i + 1]));
		}
	}


	cmd_line += " -- " + quote_arg(exe_file_name);
	cout << cmd_line << endl;
	system(cmd_line.c_str());
	string out_file = exe_file_name.substr(0, exe_file_name.length() - 4) + "_dmp.exe";
	if (check_output_file(out_file)) {
		cout << out_file << " is generated." << endl;
	}
	else {
		cout << "Failed to deobfuscate! Check log file." << endl;
	}
}
