#include "PlyParser.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <sstream>



std::string Trim(const std::string& str)
{
    size_t start = str.find_first_not_of(" \t\n\r\f\v");
    if (start == std::string::npos) {
        return ""; // 字符串全是空格
    }
    
    size_t end = str.find_last_not_of(" \t\n\r\f\v");

    return str.substr(start, end - start + 1);
}

std::vector<std::string> TokenSplit(const std::string& line)
{
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string token;
    
    while (iss >> token) {
        tokens.push_back(token);
    }
    
    return tokens;
}

bool StartsWith(const std::string& input, const std::string& prefix)
{
    if (input.length() < prefix.length()) {
        return false;
    }
    return input.compare(0, prefix.length(), prefix) == 0;
}

static bool CheckHeaderMagic(std::istream& inStream)
{
    std::string plyLine;
    std::getline(inStream, plyLine);
    plyLine = Trim(plyLine);

    return plyLine == "ply";
}

static bool ParseHeaderFormat(std::istream& inStream, PlyHeader& context)
{
    std::string plyLine;
    std::getline(inStream, plyLine);
    std::vector<std::string> tokens = TokenSplit(plyLine);
    if (tokens.size() != 3)
    {
        return false;
    }

    // "format"
    if (tokens[0] != "format")
    {
        return false;
    }

    // "type"
    const std::string &type = tokens[1];

    // ascii/binary
    if (type == "ascii") {
        context.format = PlyDataFormat::ASCII;
    } else if (type == "binary_little_endian") {
        context.format = PlyDataFormat::Binary;
    } else if (type == "binary_big_endian") {
        context.format = PlyDataFormat::BinaryBigEndian;
    } else {
        return false;
    }

    // version
    return tokens[2] == "1.0";
}

static std::unique_ptr<PlyProperty> CreatePropertyWithType(const std::string& name, const std::string& type)
{
    if (type == "uchar" || type == "uint8") {
        return std::unique_ptr<PlyProperty>(new PlyPropertyType<uint8_t>(name));
    }

    if (type == "char" || type == "int8") {
        return std::unique_ptr<PlyProperty>(new PlyPropertyType<int8_t>(name));
    }

    if (type == "ushort" || type == "uint16") {
        return std::unique_ptr<PlyProperty>(new PlyPropertyType<uint16_t>(name));
    }

    if (type == "short" || type == "int16") {
        return std::unique_ptr<PlyProperty>(new PlyPropertyType<int16_t>(name));
    }

    if (type == "uint" || type == "uint32") {
        return std::unique_ptr<PlyProperty>(new PlyPropertyType<uint32_t>(name));
    }

    if (type == "int" || type == "int32") {
        return std::unique_ptr<PlyProperty>(new PlyPropertyType<int32_t>(name));
    }

    if (type == "float" || type == "float32") {
        return std::unique_ptr<PlyProperty>(new PlyPropertyType<float>(name));
    }

    return nullptr;
}

static void ParseHeaderBody(std::istream& inStream, PlyHeader& context)
{
    while (inStream.good()) {
        std::string line;
        std::getline(inStream, line);

        // Parse a comment
        if (StartsWith(line, "comment")) {
            continue;
        }
        
        // obj info
        if (StartsWith(line, "obj_info")) {
            continue;
        }

        // element
        if (StartsWith(line, "element"))
        {
            std::vector<std::string> tokens = TokenSplit(line);
            if (tokens.size() != 3) {
                return;
            }

            const std::string &name = tokens[1];
            std::istringstream iss(tokens[2]);

            uint32_t vertexCount = 0;
            iss >> vertexCount;
            
            context.elements.emplace_back(name, vertexCount);
            continue;
        }

        // property
        if (StartsWith(line, "property"))
        {
            std::vector<std::string> tokens = TokenSplit(line);
            if (tokens.size() != 3) {
                return;
            }
            
            if (context.elements.empty()) {
                return;
            }
            const std::string &type = tokens[1];
            const std::string &name = tokens[2];

            auto prop = CreatePropertyWithType(name , type);
            context.elements.back().properties.emplace_back(std::move(prop));
            continue;
        }

        // Parse end of header
        if (StartsWith(line, "end_header")) {
            break;
        }
    }
}

static void ParseHeader(std::istream& inStream, PlyHeader& context)
{
    // check first ply
    if (!CheckHeaderMagic(inStream)) {
        return;
    }
    

    // second line is version
    if (!ParseHeaderFormat(inStream, context)) {
        return;
    }

    // parse data
    ParseHeaderBody(inStream, context);
}

static bool IsLittleEndian() {
    int32_t oneVal = 0x1;
    char* numPtr = (char*)&oneVal;
    return (numPtr[0] == 1);
}

static void ParseBinary(std::istream& inStream, const PlyHeader& context)
{
    if (!IsLittleEndian()) {
        return;
    }

    // Read all elements
    for (auto& elem : context.elements) {

        for (size_t iP = 0; iP < elem.properties.size(); iP++) {
            elem.properties[iP]->Reserve(elem.count);
        }
        for (size_t iEntry = 0; iEntry < elem.count; iEntry++) {
            for (size_t iP = 0; iP < elem.properties.size(); iP++) {
                elem.properties[iP]->Load(inStream, iEntry);
            }
        }
    }
}

static void ParseData(std::istream& inStream, const PlyHeader& context)
{
    if (context.format == PlyDataFormat::Binary) {
        ParseBinary(inStream, context);
    }
    else if (context.format == PlyDataFormat::BinaryBigEndian) {
    }
    else if (context.format == PlyDataFormat::ASCII) {
    }
}

void PlyObject::LoadFromFile(const std::string& filename)
{
    std::fstream stream(filename, std::ios::in | std::ios::binary);

    ParseHeader(stream, header);

    ParseData(stream, header);
}