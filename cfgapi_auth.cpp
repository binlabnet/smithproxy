/*
    Smithproxy- transparent proxy with SSL inspection capabilities.
    Copyright (c) 2014, Ales Stibal <astib@mag0.net>, All rights reserved.

    Smithproxy is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Smithproxy is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Smithproxy.  If not, see <http://www.gnu.org/licenses/>.

    Linking Smithproxy statically or dynamically with other modules is
    making a combined work based on Smithproxy. Thus, the terms and
    conditions of the GNU General Public License cover the whole combination.

    In addition, as a special exception, the copyright holders of Smithproxy
    give you permission to combine Smithproxy with free software programs
    or libraries that are released under the GNU LGPL and with code
    included in the standard release of OpenSSL under the OpenSSL's license
    (or modified versions of such code, with unchanged license).
    You may copy and distribute such a system following the terms
    of the GNU GPL for Smithproxy and the licenses of the other code
    concerned, provided that you include the source code of that other code
    when and as the GNU GPL requires distribution of source code.

    Note that people who make modified versions of Smithproxy are not
    obligated to grant this special exception for their modified versions;
    it is their choice whether to do so. The GNU General Public License
    gives permission to release a modified version without this exception;
    this exception also makes it possible to release a modified version
    which carries forward this exception.
*/


#include <authfactory.hpp>
#include <logger.hpp>
#include <cfgapi.hpp>


IdentityInfoBase::IdentityInfoBase() {
    idle_timeout = AuthFactory::get().global_idle_timeout;
    created = time(nullptr);
    last_seen_at = created;
}

int AuthFactory::shm_ip4_table_refresh()  {


    {
        std::lock_guard<std::recursive_mutex> l_(CfgFactory::lock());
        shm_ip4_map.attach(string_format(AUTH_IP_MEM_NAME, CfgFactory::get().tenant_name.c_str()).c_str(), AUTH_IP_MEM_SIZE,
                               string_format(AUTH_IP_SEM_NAME, CfgFactory::get().tenant_name.c_str()).c_str() );
    }
    DEBS_("cfgapi_auth_shm_ip_table_refresh: acquiring semaphore");
    int rc = shm_ip4_map.acquire();
    DEBS_("cfgapi_auth_shm_ip_table_refresh: acquiring semaphore: done");
    if(rc) {
        WARS_("cfgapi_auth_shm_ip_table_refresh: cannot acquire semaphore for token table");
        return -1;
    }
    
    DEBS_("cfgapi_auth_shm_ip_table_refresh: loading table");
    int l_ip = shm_ip4_map.load();
    DIAS_("shared memory ip user table loaded");
    DEBS_("cfgapi_auth_shm_ip_table_refresh: loading table: done, releasing semaphore");
    shm_ip4_map.release();
    DEBS_("cfgapi_auth_shm_ip_table_refresh: semaphore released");
    
    if(l_ip >= 0) {
        // new data!
        std::scoped_lock<std::recursive_mutex> l(AuthFactory::get_ip4_lock());
        
        if(l_ip == 0 && ( ! ip4_map_.empty() )) {
            DIAS_("cfgapi_auth_shm_ip_table_refresh: zero sized table received, flusing ip map");
            ip4_map_.clear();
        }
        
        DIA_("cfgapi_auth_shm_ip_table_refresh: new data: version %d, entries %d",shm_ip4_map.header_version(),shm_ip4_map.header_entries());
        for(auto& rt: shm_ip4_map.entries()) {

            std::string ip = rt.ip();
            
            auto found = ip4_map_.find(ip);
            if(found != ip4_map_.end()) {
                DIA_("Updating identity in database: %s",ip.c_str());
                IdentityInfo& id = (*found).second;
                id.ip = ip;
                id.last_logon_info = rt;
                id.username = rt.username();
                id.update();
            } else {
                IdentityInfo i;
                i.ip = ip;
                i.last_logon_info = rt;
                i.username = rt.username();
                i.update();
                ip4_map_[ip] = i;
                INF_("New identity in database: ip: %s, username: %s, groups: %s ",ip.c_str(),i.username.c_str(),i.groups.c_str());
            }
            DIA_("cfgapi_auth_shm_ip_table_refresh: loaded: %d,%s,%s",ip.c_str(),rt.username().c_str(),rt.groups().c_str());
        }

        return l_ip;
    }
    return 0;
}

int AuthFactory::shm_token_table_refresh()  {

    {
        std::lock_guard<std::recursive_mutex> l_(CfgFactory::lock());

        shm_token_map_.attach(string_format(AUTH_TOKEN_MEM_NAME, CfgFactory::get().tenant_name.c_str()).c_str(),
                                  AUTH_TOKEN_MEM_SIZE,
                                  string_format(AUTH_TOKEN_SEM_NAME, CfgFactory::get().tenant_name.c_str()).c_str());
    }

    DEBS_("cfgapi_auth_shm_token_table_refresh: acquiring semaphore");
    int rc = shm_token_map_.acquire();
    DEBS_("cfgapi_auth_shm_token_table_refresh: acquiring semaphore: done");
    if(rc) {
        WARS_("cfgapi_auth_shm_token_table_refresh: cannot acquire semaphore for token table");
        return -1;
    }
    
    DEBS_("cfgapi_auth_shm_token_table_refresh: loading table");
    int l_tok = shm_token_map_.load();
    DIAS_("shared memory auth tokem table loaded");
    DEBS_("cfgapi_auth_shm_token_table_refresh: loading table: done, releasing semaphore");
    shm_token_map_.release();
    DEBS_("cfgapi_auth_shm_token_table_refresh: semaphore released");

    if(l_tok > 0) {
        DIA_("cfgapi_auth_shm_token_table_refresh: new data: version %d, entries %d, rowsize %d",shm_token_map_.header_version(),shm_token_map_.header_entries(),shm_token_map_.header_rowsize());
        return l_tok;
    } else {
        DEB_("cfgapi_auth_shm_token_table_refresh: same data: version %d, entries %d, rowsize %d",shm_token_map_.header_version(),shm_token_map_.header_entries(),shm_token_map_.header_rowsize());
    }
    
    return 0;
};

IdentityInfo* AuthFactory::ip4_get(std::string& host) {
    IdentityInfo* ret = nullptr;

    std::scoped_lock<std::recursive_mutex> l(AuthFactory::get_ip4_lock());
    auto ip = ip4_map_.find(host);

    if (ip != ip4_map_.end()) {
        IdentityInfo& id = (*ip).second;
        ret  = &id;
    }

    return ret;
   
}


bool  AuthFactory::ip4_inc_counters(std::string& host, unsigned int rx, unsigned int tx) {
    bool ret = false;

    std::scoped_lock<std::recursive_mutex> l(AuthFactory::get_ip4_lock());
    auto ip = ip4_map_.find(host);

    if (ip != ip4_map_.end()) {
        IdentityInfo& id = (*ip).second;
        id.rx_bytes += rx;
        id.tx_bytes += tx;
        ret = true;
    }
    
    return ret;
}



// remove IP from AUTH IP MAP and synchronize with SHM AUTH IP TABLE (table which is used to communicate with bend daemon)
void AuthFactory::ip4_remove(std::string& host) {

    std::scoped_lock<std::recursive_mutex> l(AuthFactory::get_ip4_lock());
    auto ip = ip4_map_.find(host);

    if (ip != ip4_map_.end()) {
        // erase internal ip map entry

        DEB_("cfgapi_ip_map_remove: auth ip map - removing: %s",host.c_str());
        ip4_map_.erase(ip);

        // for debug only: print all shm table entries
        if(LEV_(DUM)) {
            DUMS_(":: - SHM AUTH IP map - before removal::");
            for(auto& x_it: shm_ip4_map.map_entries()) {
                DUM_("::  %s::",x_it.first.c_str());
            }
        }
        
        shm_ip4_map.acquire();

        // erase shared ip map entry
        auto sh_it = shm_ip4_map.map_entries().find(host);

        if(sh_it != shm_ip4_map.map_entries().end()) {
            DEB_("cfgapi_ip_map_remove: shm auth ip table  - removing: %s",host.c_str());
            shm_ip4_map.map_entries().erase(sh_it);
            
            if(LEV_(DUM)) {
                DUMS_(":: - SHM AUTH IP map - after removal::");
                for(auto& x_it: shm_ip4_map.map_entries()) {
                        DUM_("::   %s::",x_it.first.c_str());
                }                
                
            }
            shm_ip4_map.save(true);
        }
        shm_ip4_map.release();
    }
}


void AuthFactory::ip4_timeout_check() {
    DEBS_("cfgapi_ip_auth_timeout_check: started");
    std::scoped_lock<std::recursive_mutex> l(AuthFactory::get_ip4_lock());
    
    std::set<std::string> to_remove;
    
    for(auto& e: ip4_map_) {
        const std::string&  ip = e.first;
        IdentityInfo& id = e.second;
        
        DEB_("cfgapi_ip_auth_timeout_check: %s", ip.c_str());
        if(id.i_timeout()) {
            DIA_("cfgapi_ip_auth_timeout_check: idle timeout, adding to list %s", ip.c_str());
            to_remove.insert(ip);
        }
    }
    
    for(auto tr: to_remove) {
        ip4_remove(tr);
        INF_("IP address %s identity timed out.", tr.c_str());
    }
}


bool AuthFactory::ipX_inc_counters(baseHostCX* cx, unsigned int rx, unsigned int tx) {
    if(cx && cx->com()) {
        if(cx->com()->l3_proto() == AF_INET6) {
            return ip6_inc_counters(cx->host(),rx,tx);
        } else
        if(cx->com()->l3_proto() == AF_INET || cx->com()->l3_proto() == 0)  {
            return ip4_inc_counters(cx->host(),rx,tx);
        }
    }
    
    return false;
}

bool AuthFactory::ipX_inc_counters(baseHostCX* cx) {
    if(cx) {
        return ipX_inc_counters(cx,cx->meter_read_bytes,cx->meter_write_bytes);
    }
    
    return false;
}
