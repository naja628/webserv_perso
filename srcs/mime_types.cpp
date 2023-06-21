#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>

typedef std::map<const std::string, const std::string> MimeMap;
typedef std::pair<const std::string, const std::string> ConstPair;

void parseMimeTypes(const std::string& filePath, MimeMap& mimeTypes)
{
	std::ifstream file(filePath.c_str());
	if (!file)
	{
		std::cerr << "Error opening file: " << filePath << std::endl;
		return;
	}

	std::string line;
	bool parsingBlock = false;
	while (std::getline(file, line))
	{
		if (line.empty() || line[0] == '#')
			continue ;

		std::istringstream iss(line);
		std::string key;
		std::string extension;

		if (!(iss >> key >> extension))
		{
			if (key == "}" && parsingBlock)
				return ;
			std::cerr << "Error parsing line: " << line << std::endl;
			return ;
		}

		if (key == "types" && extension == "{")
		{
			if (parsingBlock)
			{
				std::cerr << "Unexpected '{' encountered." << std::endl;
				return ;
			}
			parsingBlock = true;
			continue ;
		}

		if (extension.empty() || extension[extension.length() - 1] != ';')
		{
			std::cerr << "Missing ';' at the end of line: " << line << std::endl;
			return ;
		}
		extension = extension.substr(0, extension.length() - 1);

		mimeTypes.insert(ConstPair(extension, key));
	}

	file.close();
}
