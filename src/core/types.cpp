#include "lazycmake/core/types.hpp"

#include <stdexcept>

namespace lazycmake::core {

std::string toString(Language value) {
    switch (value) {
        case Language::C: return "C";
        case Language::Cpp: return "Cpp";
        case Language::Both: return "Both";
    }
    throw std::invalid_argument("Unknown Language enum value");
}

std::string toString(CppStandard value) {
    switch (value) {
        case CppStandard::Cpp17: return "Cpp17";
        case CppStandard::Cpp20: return "Cpp20";
        case CppStandard::Cpp23: return "Cpp23";
    }
    throw std::invalid_argument("Unknown CppStandard enum value");
}

std::string toString(CStandard value) {
    switch (value) {
        case CStandard::C11: return "C11";
        case CStandard::C17: return "C17";
    }
    throw std::invalid_argument("Unknown CStandard enum value");
}

std::string toString(TargetKind value) {
    switch (value) {
        case TargetKind::Executable: return "Executable";
        case TargetKind::StaticLibrary: return "StaticLibrary";
        case TargetKind::SharedLibrary: return "SharedLibrary";
        case TargetKind::InterfaceLibrary: return "InterfaceLibrary";
    }
    throw std::invalid_argument("Unknown TargetKind enum value");
}

std::string toString(BuildType value) {
    switch (value) {
        case BuildType::Debug: return "Debug";
        case BuildType::Release: return "Release";
        case BuildType::RelWithDebInfo: return "RelWithDebInfo";
        case BuildType::MinSizeRel: return "MinSizeRel";
    }
    throw std::invalid_argument("Unknown BuildType enum value");
}

std::string toString(GeneratorKind value) {
    switch (value) {
        case GeneratorKind::Ninja: return "Ninja";
        case GeneratorKind::UnixMakefiles: return "UnixMakefiles";
        case GeneratorKind::NMake: return "NMake";
    }
    throw std::invalid_argument("Unknown GeneratorKind enum value");
}

std::string toString(DependencySource value) {
    switch (value) {
        case DependencySource::FetchContent: return "FetchContent";
        case DependencySource::CPM: return "CPM";
        case DependencySource::VcpkgFindPackage: return "VcpkgFindPackage";
        case DependencySource::SystemFindPackage: return "SystemFindPackage";
    }
    throw std::invalid_argument("Unknown DependencySource enum value");
}

Language languageFromString(const std::string& value) {
    if (value == "C") return Language::C;
    if (value == "Cpp") return Language::Cpp;
    if (value == "Both") return Language::Both;
    throw std::invalid_argument("Unknown Language string: " + value);
}

CppStandard cppStandardFromString(const std::string& value) {
    if (value == "Cpp17") return CppStandard::Cpp17;
    if (value == "Cpp20") return CppStandard::Cpp20;
    if (value == "Cpp23") return CppStandard::Cpp23;
    throw std::invalid_argument("Unknown CppStandard string: " + value);
}

CStandard cStandardFromString(const std::string& value) {
    if (value == "C11") return CStandard::C11;
    if (value == "C17") return CStandard::C17;
    throw std::invalid_argument("Unknown CStandard string: " + value);
}

TargetKind targetKindFromString(const std::string& value) {
    if (value == "Executable") return TargetKind::Executable;
    if (value == "StaticLibrary") return TargetKind::StaticLibrary;
    if (value == "SharedLibrary") return TargetKind::SharedLibrary;
    if (value == "InterfaceLibrary") return TargetKind::InterfaceLibrary;
    throw std::invalid_argument("Unknown TargetKind string: " + value);
}

BuildType buildTypeFromString(const std::string& value) {
    if (value == "Debug") return BuildType::Debug;
    if (value == "Release") return BuildType::Release;
    if (value == "RelWithDebInfo") return BuildType::RelWithDebInfo;
    if (value == "MinSizeRel") return BuildType::MinSizeRel;
    throw std::invalid_argument("Unknown BuildType string: " + value);
}

GeneratorKind generatorKindFromString(const std::string& value) {
    if (value == "Ninja") return GeneratorKind::Ninja;
    if (value == "UnixMakefiles") return GeneratorKind::UnixMakefiles;
    if (value == "NMake") return GeneratorKind::NMake;
    throw std::invalid_argument("Unknown GeneratorKind string: " + value);
}

DependencySource dependencySourceFromString(const std::string& value) {
    if (value == "FetchContent") return DependencySource::FetchContent;
    if (value == "CPM") return DependencySource::CPM;
    if (value == "VcpkgFindPackage") return DependencySource::VcpkgFindPackage;
    if (value == "SystemFindPackage") return DependencySource::SystemFindPackage;
    throw std::invalid_argument("Unknown DependencySource string: " + value);
}

std::string toCMakeFeatureString(CppStandard value) {
    switch (value) {
        case CppStandard::Cpp17: return "cxx_std_17";
        case CppStandard::Cpp20: return "cxx_std_20";
        case CppStandard::Cpp23: return "cxx_std_23";
    }
    throw std::invalid_argument("Unknown CppStandard enum value");
}

std::string toCMakeFeatureString(CStandard value) {
    switch (value) {
        case CStandard::C11: return "c_std_11";
        case CStandard::C17: return "c_std_17";
    }
    throw std::invalid_argument("Unknown CStandard enum value");
}

}  // namespace lazycmake::core
