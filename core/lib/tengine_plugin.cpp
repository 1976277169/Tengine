/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * License); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * AS IS BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/*
 * Copyright (c) 2017, Open AI Lab
 * Author: jingyou@openailab.com
 */
#include "tengine_plugin.hpp"
#include "tengine_config.hpp"

namespace TEngine {

using NameManager = Attribute;
using InitManager = Attribute;
using HandlerManager = Attribute;
using ConfManager = Attribute;

NameManager * TEnginePlugin::GetNameManager(void)
{
	static NameManager instance;

	return &instance;
}

TEnginePlugin::PrioManager * TEnginePlugin::GetPrioManager(void)
{
        static PrioManager instance;
        return &instance;
}

InitManager * TEnginePlugin::GetInitManager(void)
{
	static InitManager instance;

	return &instance;
}

HandlerManager * TEnginePlugin::GetHandlerManager(void)
{
	static HandlerManager instance;

	return &instance;
}

void TEnginePlugin::SetPluginManager(void)
{
	ConfManager *cm = TEngineConfig::GetConfManager();
	std::vector<std::string> keys;
	std::string value;

	// Get the list of keys
	keys = cm->ListAttr();
	for(unsigned int i=0; i < keys.size(); i++)
	{
		value = "";
		if(!TEngineConfig::Get<std::string>(keys.at(i), value))
			continue;
		// Set the plugin manager
		SetPlugin(keys.at(i), value);
	}
}

void TEnginePlugin::SetPlugin(const std::string& key, const std::string& value)
{
	NameManager *nm= GetNameManager();
	InitManager *im= GetInitManager();
        PrioManager *pm= GetPrioManager();

	std::vector<std::string> subkey;

	subkey = TEngineConfig::ParseKey(key);
	if(subkey.size() > 2 && subkey.at(0) == "plugin")
	{
		if(subkey.at(2) == "so")
		{
			nm->SetAttr(subkey.at(1), value);
		}
		else if(subkey.at(2) == "init")
		{
			im->SetAttr(subkey.at(1), value);
		}
                else if(subkey.at(2) =="prio")
                {
                   int prio=strtoul(value.c_str(),NULL,10);
                   (*pm)[prio]=subkey.at(1);
                }
                
	}
}

bool TEnginePlugin::LoadAll(void)
{
	NameManager *nm = GetNameManager();
	std::vector<std::string> names;
	names = nm->ListAttr();

        PrioManager *pm= GetPrioManager();

        auto ir=pm->begin();
        
        while(ir!=pm->end())
        {
            const std::string& key=ir->second;

	    if(!LoadPlugin(key))
			return  false;

	    if(!InitPlugin(key))
			return false;

             ir++;
	}

	return true;
}

bool TEnginePlugin::LoadPlugin(const std::string& name)
{
	HandlerManager *hm = GetHandlerManager();

	std::string fullname;
	ShareLibParser *p = new ShareLibParser();

	if(!GetFullName(name, fullname))
		return false;

	if(!fullname.size() || p->Load(fullname) < 0)
	{
		LOG_ERROR()<<"Failed in loading the plugin!\n";
		return false;
	}

	hm->SetAttr(name, p);
	LOG_INFO()<<"Successfully load plugin : "<<fullname<<"\n";
	return true;
}

bool TEnginePlugin::InitPlugin(const std::string& name)
{
	std::string initname;
	ShareLibParser p;

	if(!GetInitName(name, initname))
		return false;

	if(!initname.size() || !GetHandler(name, p))
		return false;

	p.ExcecuteFunc<int()>(initname);
	return true;
}

bool TEnginePlugin::GetFullName(const std::string& name, std::string& fullname)
{
	NameManager *nm = GetNameManager();
	if(!nm->ExistAttr(name))
	{
		LOG_ERROR()<<"The plugin is not set in the config file!\n";
		return false;
	}

	nm->GetAttr<std::string>(name, &fullname);

	if(!fullname.size())
	{
		LOG_ERROR()<<"The fullname of the plugin is not set in the config file!\n";
		return false;
	}
	return true;
}

bool TEnginePlugin::GetInitName(const std::string& name, std::string& initname)
{
	InitManager *im = GetInitManager();
	if(!im->ExistAttr(name))
	{
		LOG_ERROR()<<"The plugin is not set in the config file!\n";
		return false;
	}

	im->GetAttr<std::string>(name, &initname);

	if(!initname.size())
	{
		LOG_ERROR()<<"The initname of the plugin is not set in the config file!\n";
		return false;
	}
	return true;
}

bool TEnginePlugin::GetHandler(const std::string& name, ShareLibParser& handler)
{
	HandlerManager *hm = GetHandlerManager();
	if(!hm->ExistAttr(name))
	{
		LOG_ERROR()<<"The handler is not got yet!\n";
		return false;
	}

	handler = *any_cast<ShareLibParser *>(hm->GetAttr(name));
	return true;
}

void TEnginePlugin::DumpPlugin(void)
{
	NameManager *nm = GetNameManager();
	InitManager *im = GetInitManager();
	std::vector<std::string> names;

	names = nm->ListAttr();
	for(unsigned int i=0; i < names.size(); i++)
	{
		std::string fullname, initname;
		nm->GetAttr(names.at(i), &fullname);
		im->GetAttr(names.at(i), &initname);
		std::cout<<names.at(i)<<" : "<<fullname<<" : "<<initname<<std::endl;
	}
}

std::unordered_map<int,module_init_func_t> * TEnginePlugin::GetInitTable(void)
{
	static std::unordered_map<int, module_init_func_t> instance;

	return &instance;
}

void TEnginePlugin::RegisterModuleInit(int priority, module_init_func_t init_func)
{
	std::unordered_map<int, module_init_func_t> * p_table=GetInitTable();

	(*p_table)[priority]=init_func;

}

std::unordered_map<int,module_release_func_t> * TEnginePlugin::GetReleaseTable(void)
{
	static std::unordered_map<int, module_release_func_t> instance;

	return &instance;
}

void TEnginePlugin::RegisterModuleRelease(int priority, module_release_func_t rel_func)
{
	std::unordered_map<int, module_release_func_t> * p_table=GetReleaseTable();

	(*p_table)[priority]=rel_func;
}

void TEnginePlugin::InitModule(void)
{
	std::unordered_map<int, module_init_func_t> * p_table=GetInitTable();
	auto ir=p_table->begin();
	auto end=p_table->end();

	while(ir!=end)
	{
		module_init_func_t func=ir->second;
		func();
		ir++;
	}
}

void TEnginePlugin::ReleaseModule(void)
{
        std::unordered_map<int, module_release_func_t> * p_table=GetReleaseTable();
        auto ir=p_table->begin();
        auto end=p_table->end();

        std::vector<module_release_func_t> func_list;
        
        while(ir!=end)
        {
             func_list.push_back(ir->second);
             ir++;
        }

        auto r_start=func_list.rbegin();
        auto r_end=func_list.rend();

        while(r_start!=r_end)
        {
             (*r_start)();
             r_start++;
        }
        
}


} //end of namespace TEngine
