#pragma once
class HCMDirPath {
private:
	static std::string dirPath;
	static bool dirPathSet;

public:
	static void SetHCMDirPath(std::string in)
	{
		if (dirPathSet) throw HCMInitException("dirPath already set!");
		dirPath = in.ends_with("\\") ? in : (in + "\\"); // make sure ends in backslash
		dirPathSet = true;
	}

	static std::string_view GetHCMDirPath() 
	{ 
		if (!dirPathSet) throw HCMRuntimeException("dirPath not set!");
		return dirPath; 
	}
};