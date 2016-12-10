
#include<vector>
#include<string>

std::vector<std::string> split(const std::string& str, const std::string& delim)
{
    std::vector<std::string> result;
    size_t  start = 0, end = 0;

    while (end != string::npos)
    {
        end = str.find( delim, start);

        result.push_back(str.substr(start, (end == string::npos) ? string::npos : end - start));

        start = ((end > (string::npos - delim.size())) ? string::npos : end + delim.size());
    }

    return result;
}


