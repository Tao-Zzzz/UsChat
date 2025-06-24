#pragma once
#include "const.h"
#include <fstream>  
#include <boost/property_tree/ptree.hpp>  
#include <boost/property_tree/ini_parser.hpp>  
#include <boost/filesystem.hpp>    
#include <map>
#include <iostream>

// 把config所有参数扔到一个map里,用一个大map存储小section
struct SectionInfo {
	SectionInfo(){}
	~SectionInfo(){}

	// 拷贝构造
	SectionInfo(const SectionInfo& src) {
		_section_datas = src._section_datas;
	}
	// 拷贝赋值
	SectionInfo& operator = (const SectionInfo& src) {
		//不允许自己拷贝自己
		if (&src == this) {
			return *this;
		}
		this->_section_datas = src._section_datas;
		return *this;
	}

	std::map<std::string, std::string> _section_datas;
	std::string operator[](const std::string& key) {
		if (_section_datas.find(key) == _section_datas.end()) {
			return "";
		}
		return _section_datas[key];
	}


};

// 管理section
class ConfigMgr
{
public:
	~ConfigMgr() {
		_config_map.clear();
	}

	// 传的是section的名字
	SectionInfo operator[](const std::string& section) {
		if (_config_map.find(section) == _config_map.end()) {
			return SectionInfo();
		}

		return _config_map[section];
	}

	// c++11只后,局部静态只会进行一次初始化,而且线程安全
	static ConfigMgr& Inst() {
		// 生命周期与程序同步,可见范围为局部作用域
		static ConfigMgr cfg_mgr;
		return cfg_mgr;
	}

	// 拷贝构造
	ConfigMgr(const ConfigMgr& src) {
		_config_map = src._config_map;
	}
	// 拷贝赋值
	ConfigMgr& operator = (const ConfigMgr& src) {
		//不允许自己拷贝自己
		if (&src == this) {
			return *this;
		}
		this->_config_map = src._config_map;
	}
private:
	ConfigMgr();
	std::map<std::string, SectionInfo> _config_map;
};

