#pragma once

#include <algorithm>
#include <string>
#include <vector>
#include <memory>
#include <iostream>

enum class PlyDataFormat : uint8_t
{
    ASCII,
    Binary,
    BinaryBigEndian
};

struct PlyProperty
{
    explicit PlyProperty(const std::string& name_) : name(name_) {}
    virtual ~PlyProperty() = default;

    virtual void Reserve(size_t count_) = 0;
    virtual void Load(std::istream& archive, size_t idx) = 0;

    virtual const float* GetAsFloat(size_t index) const = 0;

    std::string name;
};

template <typename T>
struct PlyPropertyType : public PlyProperty
{
    explicit PlyPropertyType(const std::string& name_) : PlyProperty(name_) {}

    void Reserve(size_t count_) override { data.resize(count_); }

    void Load(std::istream& archive, size_t idx) override
    {
        archive.rdbuf()->sgetn((char*)&data[idx], sizeof(T));
    }

    const float* GetAsFloat(size_t index) const override
    {
        return (const float*)(&data[index]);
    }
    
    std::vector<T> data;
};

struct PlyElement
{
    PlyElement(const std::string& name_, size_t count_) : name(name_), count(count_) {}

    std::string name;
    size_t count;
    std::vector<std::unique_ptr<PlyProperty>> properties;
};

struct PlyHeader
{
    PlyDataFormat format;
    std::string version;
    std::vector<PlyElement> elements;
};

class PlyObject
{
public:
    PlyObject() = default;
    ~PlyObject() = default;

    void LoadFromFile(const std::string& filename);
    
    const PlyHeader &GetData() const { return header; }
private:
    PlyHeader header = {};
};


bool StartsWith(const std::string& input, const std::string& prefix);