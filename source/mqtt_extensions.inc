/* MQTT controls and diagnostics. Included after the protocol/discovery helpers. */
static u64 ext_connections, ext_session_started, ext_game_started, ext_game_id, ext_next_poll;
static bool ext_ts, ext_nifm;
static Service ext_omm_service;

static bool ext_docked(bool *docked) {
    if(!serviceIsActive(&ext_omm_service)) smGetService(&ext_omm_service,"omm");
    u8 mode=0;
    if(!serviceIsActive(&ext_omm_service))return false;
    Result rc=serviceDispatchOut(&ext_omm_service,0,mode);
    if(R_SUCCEEDED(rc))*docked=mode==1;
    return R_SUCCEEDED(rc);
}

static void ext_escape(const char *in, char *out, size_t size) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j + 7 < size; i++) {
        unsigned char c = in[i];
        if (c == '"' || c == '\\') { out[j++] = '\\'; out[j++] = c; }
        else if (c < 32) { j += snprintf(out+j, size-j, "\\u%04x", c); }
        else out[j++] = c;
    }
    out[j] = 0;
}

static const char *ext_capability(const char *id) {
    if(strcmp(id,"game")==0 || strcmp(id,"game_2p")==0 || strcmp(id,"game_4p")==0 || strcmp(id,"game_8p")==0)return "launch";
    if(strstr(id,"brightness") || strcmp(id,"screen")==0 || strcmp(id,"backlight")==0) return "lbl";
    if(strstr(id,"volume") || strcmp(id,"mute")==0 || strcmp(id,"audio_target")==0) return "audctl";
    if(strncmp(id,"player_",7)==0 || strcmp(id,"controller_count")==0)return "hid";
    if(strcmp(id,"soc_temperature")==0)return "ts";
    if(strcmp(id,"wifi_bars")==0 || strcmp(id,"connection_type")==0)return "nifm";
    return NULL;
}
static void ext_availability(const AppConfig*c,const char*id,char*json,size_t size) {
    char device[192],service[192],suffix[40];
    state_topic(c,"availability",device,sizeof(device));
    size_t used=strlen(json);if(!used)return;
    const char*cap=ext_capability(id);
    if(cap){
        snprintf(suffix,sizeof(suffix),"capability/%s",cap);state_topic(c,suffix,service,sizeof(service));
        snprintf(json+used-1,size-used+1,",\"avty_mode\":\"all\",\"avty\":[{\"t\":\"%s\"},{\"t\":\"%s\"}]}",device,service);
    }else snprintf(json+used-1,size-used+1,",\"avty_t\":\"%s\"}",device);
}

static void ext_entity(int fd, const AppConfig *c, const char *type, const char *id,
                       const char *name, const char *extra, bool control) {
    char topic[256], stat[192], cmd[192], safe[80], escaped[400], payload[MQTT_BUFFER];
    discovery_topic(c, type, id, topic, sizeof(topic));
    state_topic(c, id, stat, sizeof(stat));
    char suffix[100]; snprintf(suffix, sizeof(suffix), "control/%s", id);
    state_topic(c, suffix, cmd, sizeof(cmd));
    sanitize_id(client_id_or_device(c), safe, sizeof(safe));
    ext_escape(c->device_name, escaped, sizeof(escaped));
    snprintf(payload, sizeof(payload), "{\"name\":\"%s\",\"uniq_id\":\"%s_ext_%s\",\"stat_t\":\"%s\",\"dev\":{\"ids\":[\"%s\"],\"name\":\"%s\",\"mf\":\"Nintendo\",\"mdl\":\"Switch\",\"sw_version\":\"%s\"}%s%s%s%s}", name,safe,id,stat,safe,escaped,SHA_APP_BUILD,control?",\"cmd_t\":\"":"",control?cmd:"",control?"\"":"",extra);
    /* Include service availability for entities that require optional services. */
    if(strcmp(type,"button")==0) {
        char *start=strstr(payload,",\"stat_t\":");
        if(start) {char *end=strstr(start+1,",\"dev\":");if(end)memmove(start,end,strlen(end)+1);}
    }
    ext_availability(c,id,payload,sizeof(payload));
    mqtt_send_publish(fd, topic, payload, true);
}

static void extended_discovery(AppState *state, int fd, const AppConfig *c) {
    (void)state;
    ext_connections++;
    ext_session_started = monotonic_ms();
    ext_next_poll=0;
    ext_entity(fd,c,"number","set_brightness","Brightness control",",\"min\":0,\"max\":100,\"step\":1,\"unit_of_meas\":\"%\"",true);
    ext_entity(fd,c,"number","set_volume","Volume control",",\"min\":0,\"max\":100,\"step\":1,\"unit_of_meas\":\"%\"",true);
    ext_entity(fd,c,"switch","screen","Screen",",\"pl_on\":\"ON\",\"pl_off\":\"OFF\"",true);
    ext_entity(fd,c,"switch","mute","Mute",",\"pl_on\":\"ON\",\"pl_off\":\"OFF\"",true);
    clear_discovery_topic(fd,c,"switch","stream_enabled");
    clear_discovery_topic(fd,c,"button","sleep");
    clear_discovery_topic(fd,c,"button","close_game");
    ext_entity(fd,c,"sensor","stream_clients","RTSP clients","",false);
    ext_entity(fd,c,"sensor","stream_error","RTSP capture result","",false);
    const char *buttons[][2]={{"volume_up","Volume up"},{"volume_down","Volume down"},{"get_logs","Fetch diagnostics"},{"refresh","Refresh sensors"},{"refresh_games","Refresh installed games"}};
    for(size_t i=0;i<sizeof(buttons)/sizeof(buttons[0]);i++) ext_entity(fd,c,"button",buttons[i][0],buttons[i][1],",\"payload_press\":\"PRESS\"",true);
    ext_entity(fd,c,"event","overlay_action","Overlay action",",\"event_types\":[\"quick_action_1\",\"quick_action_2\",\"quick_action_3\",\"quick_action_4\"]",false);
    const char *sensors[][3]={{"uptime","Console uptime",",\"unit_of_meas\":\"s\",\"dev_cla\":\"duration\""},{"mqtt_reconnections","MQTT reconnections",""},{"mqtt_session","MQTT session duration",",\"unit_of_meas\":\"s\""},{"soc_temperature","Internal temperature",",\"unit_of_meas\":\"°C\",\"dev_cla\":\"temperature\""},{"wifi_bars","Wi-Fi signal bars",",\"icon\":\"mdi:wifi\""},{"connection_type","Connection type",""},{"process_memory","Sysmodule memory",",\"unit_of_meas\":\"MiB\""},{"sd_total","SD capacity",",\"unit_of_meas\":\"GiB\""},{"internal_free","Internal free space",",\"unit_of_meas\":\"GiB\""},{"internal_total","Internal capacity",",\"unit_of_meas\":\"GiB\""},{"game_session","Game session duration",",\"unit_of_meas\":\"s\""},{"stream_status","RTSP status",""},{"command_status","Last command result",""}};
    for(size_t i=0;i<sizeof(sensors)/sizeof(sensors[0]);i++) ext_entity(fd,c,"sensor",sensors[i][0],sensors[i][1],sensors[i][2],false);
    ext_entity(fd,c,"sensor","game_query_status","Game detection diagnostic",",\"ent_cat\":\"diagnostic\"",false);
    ext_entity(fd,c,"binary_sensor","docked","Docked","",false);
    char diagnostic_topic[192],diagnostic_extra[320];
    state_topic(c,"diagnostic_attributes",diagnostic_topic,sizeof(diagnostic_topic));
    snprintf(diagnostic_extra,sizeof(diagnostic_extra),",\"ent_cat\":\"diagnostic\",\"json_attr_t\":\"%s\"",diagnostic_topic);
    ext_entity(fd,c,"sensor","diagnostics","Diagnostics",diagnostic_extra,false);
    for(int i=1;i<=8;i++) {
        char id[40],name[60];
        snprintf(id,sizeof(id),"player_%d_wired",i); snprintf(name,sizeof(name),"Player %d wired",i);
        ext_entity(fd,c,"binary_sensor",id,name,"",false);
        snprintf(id,sizeof(id),"player_%d_battery",i); snprintf(name,sizeof(name),"Player %d battery level (0-4)",i);
        ext_entity(fd,c,"sensor",id,name,"",false);
    }
}

static void ext_number(int fd,const AppConfig*c,const char*id,double value) {
    char b[40]; snprintf(b,sizeof(b),"%.2f",value); publish_value(fd,c,id,b);
}
static void ext_result(int fd,const AppConfig*c,const char*cmd,Result rc) {
    char b[160]; snprintf(b,sizeof(b),"%s:%s:0x%x",cmd,R_SUCCEEDED(rc)?"success":"failed",rc);
    publish_value(fd,c,"command_status",b);
}

static void extended_poll(AppState *s,int fd,const AppConfig*c) {
    FILE *action_file=fopen("sdmc:/switch/switch-ha/overlay-command.ini","r");
    if(action_file) {
        char action[40]={0};fgets(action,sizeof(action),action_file);fclose(action_file);
        action[strcspn(action,"\r\n")]=0;
        int slot=0;
        if(sscanf(action,"quick_action_%d",&slot)==1 && slot>=1 && slot<=4) {
            char topic[192],json[90];state_topic(c,"overlay_action",topic,sizeof(topic));
            snprintf(json,sizeof(json),"{\"event_type\":\"quick_action_%d\"}",slot);
            if(mqtt_send_publish(fd,topic,json,false))remove("sdmc:/switch/switch-ha/overlay-command.ini");
        } else remove("sdmc:/switch/switch-ha/overlay-command.ini");
    }
    const char *caps[]={"lbl","audctl","hid","pminfo","ts","nifm","launch"};
    bool ready[]={s->svc_lbl_ready,s->svc_audctl_ready,s->svc_hid_ready,s->svc_pminfo_ready,ext_ts,ext_nifm,s->app_mode==AppMode_ForegroundApp && (appletGetAppletType()==AppletType_Application || appletGetAppletType()==AppletType_LibraryApplet)};
    for(size_t i=0;i<7;i++){char topic[40];snprintf(topic,sizeof(topic),"capability/%s",caps[i]);publish_value(fd,c,topic,ready[i]?"online":"offline");}
    u64 now=monotonic_ms();
    if(now<ext_next_poll) return;
    ext_next_poll=now+5000;
    ext_number(fd,c,"uptime",now/1000);
    ext_number(fd,c,"mqtt_session",(now-ext_session_started)/1000);
    ext_number(fd,c,"mqtt_reconnections",ext_connections ? ext_connections-1:0);
    u64 mem;
    if(R_SUCCEEDED(svcGetInfo(&mem,InfoType_UsedMemorySize,CUR_PROCESS_HANDLE,0))) ext_number(fd,c,"process_memory",mem/1048576.0);
    if(!ext_ts) ext_ts=R_SUCCEEDED(tsInitialize());
    s32 temp;
    if(ext_ts && R_SUCCEEDED(tsGetTemperature(TsLocation_Internal,&temp))) ext_number(fd,c,"soc_temperature",temp/1000.0);
    else publish_value(fd,c,"soc_temperature","None");
    if(!ext_nifm) ext_nifm=R_SUCCEEDED(nifmInitialize(NifmServiceType_System));
    NifmInternetConnectionType type; NifmInternetConnectionStatus status; u32 bars;
    if(ext_nifm && R_SUCCEEDED(nifmGetInternetConnectionStatus(&type,&bars,&status))) {
        ext_number(fd,c,"wifi_bars",bars);
        publish_value(fd,c,"connection_type",type==NifmInternetConnectionType_WiFi?"wifi":"ethernet");
    } else {publish_value(fd,c,"wifi_bars","None"); publish_value(fd,c,"connection_type","unknown");}
    struct statvfs v;
    if(statvfs("sdmc:/",&v)==0) ext_number(fd,c,"sd_total",(double)v.f_blocks*v.f_frsize/1073741824.0);
    /* Built-in user partition: query sizes only; never mount for writes. */
    FsFileSystem fs;
    if(R_SUCCEEDED(fsOpenBisFileSystem(&fs,FsBisPartitionId_User,""))) {
        s64 free_bytes,total_bytes;
        if(R_SUCCEEDED(fsFsGetFreeSpace(&fs,"/",&free_bytes))) ext_number(fd,c,"internal_free",free_bytes/1073741824.0);
        if(R_SUCCEEDED(fsFsGetTotalSpace(&fs,"/",&total_bytes))) ext_number(fd,c,"internal_total",total_bytes/1073741824.0);
        fsFsClose(&fs);
    }
    u64 game=0; char game_name[512];
    if(current_game(s,&game,game_name,sizeof(game_name))) {
        if(game!=ext_game_id) {ext_game_id=game;ext_game_started=now;}
        ext_number(fd,c,"game_session",(now-ext_game_started)/1000);
    } else {ext_game_id=0;ext_number(fd,c,"game_session",0);}
    char game_query_status[SHA_LOG_LINE];
    mutexLock(&s->lock);snprintf(game_query_status,sizeof(game_query_status),"%s",s->game_status);mutexUnlock(&s->lock);
    publish_value(fd,c,"game_query_status",game_query_status);
    ext_number(fd,c,"stream_clients",screen_stream_client_count());
    char result[32];snprintf(result,sizeof(result),"0x%x",screen_stream_get_grcd_begin_result());
    publish_value(fd,c,"stream_error",result);
    char stream[128]; screen_stream_get_status(stream,sizeof(stream)); publish_value(fd,c,"stream_status",stream);
    /* Charger power alone cannot prove the console is in a dock. */
    bool docked;
    publish_value(fd,c,"docked",ext_docked(&docked)?(docked?"ON":"OFF"):"None");
    if(s->svc_lbl_ready) {
        float b; LblBacklightSwitchStatus bl;
        if(R_SUCCEEDED(lblGetCurrentBrightnessSetting(&b))) ext_number(fd,c,"set_brightness",b*100);
        if(R_SUCCEEDED(lblGetBacklightSwitchStatus(&bl))) publish_value(fd,c,"screen",bl==LblBacklightSwitchStatus_Enabled?"ON":"OFF");
    }
    if(s->svc_audctl_ready) {
        AudioVolumeState audio;
        if(R_SUCCEEDED(audio_control_read(&audio))) {
            ext_number(fd,c,"set_volume",audio.percent);
            publish_value(fd,c,"mute",audio.muted?"ON":"OFF");
        }
    }
    if(s->svc_hid_ready) for(int i=0;i<8;i++) {
        HidNpadIdType id=i;
        if(i==0 && !hidGetNpadStyleSet(id)) id=HidNpadIdType_Handheld;
        u32 attributes=0;
        bool connected=npad_has_state(id,hidGetNpadStyleSet(id),&attributes);
        char key[40]; snprintf(key,sizeof(key),"player_%d_wired",i+1);
        publish_value(fd,c,key,connected && (attributes & (HidNpadAttribute_IsWired|HidNpadAttribute_IsLeftWired|HidNpadAttribute_IsRightWired))?"ON":"OFF");
        HidPowerInfo info={0};
        if(hidGetNpadStyleSet(id)&HidNpadStyleTag_NpadJoyDual) {
            HidPowerInfo right={0};hidGetNpadPowerInfoSplit(id,&info,&right);
            if(right.battery_level<info.battery_level)info=right;
        } else hidGetNpadPowerInfoSingle(id,&info);
        snprintf(key,sizeof(key),"player_%d_battery",i+1);
        if(hidGetNpadStyleSet(id)) ext_number(fd,c,key,info.battery_level); else publish_value(fd,c,key,"None");
    }
}

#define EXT_MAX_GAMES 128
typedef struct {u64 id; char label[180]; int players;} ExtGame;
static ExtGame ext_games[EXT_MAX_GAMES];
static size_t ext_game_count;

static void ext_games_refresh(int fd,const AppConfig*c) {
    Result rc=nsInitialize();
    if(R_FAILED(rc)) {ext_result(fd,c,"refresh_games",rc);return;}
    size_t count=0;
    for(s32 offset=0;count<EXT_MAX_GAMES;offset+=16) {
        NsApplicationRecord records[16];s32 n=0;
        rc=nsListApplicationRecord(records,16,offset,&n);
        if(R_FAILED(rc)||n<=0)break;
        for(s32 i=0;i<n && count<EXT_MAX_GAMES;i++) {
            char name[128];
            if(!title_cache_lookup(records[i].application_id,name,sizeof(name))) snprintf(name,sizeof(name),"Game");
            ext_games[count].id=records[i].application_id;
            ext_games[count].players=0;
            snprintf(ext_games[count].label,sizeof(ext_games[count].label),"%.120s [%016llX]",name,(unsigned long long)records[i].application_id);
            count++;
        }
        mqtt_send_ping(fd);
    }
    nsExit();
    if(R_FAILED(rc)){ext_result(fd,c,"refresh_games",rc);return;}
    ext_game_count=count;
    /* Player capacity is curated metadata, not guessed from a game's name. */
    FILE*f=fopen("sdmc:/switch/switch-ha/game-players.ini","r");
    if(f){char line[100];while(fgets(line,sizeof(line),f)){unsigned long long id;int players;
        if(sscanf(line,"%llx=%d",&id,&players)==2 && players>=1 && players<=8)
            for(size_t i=0;i<count;i++)if(ext_games[i].id==id)ext_games[i].players=players;
    }fclose(f);}
    char *extra=malloc(60000);
    if(!extra){ext_result(fd,c,"refresh_games",MAKERESULT(Module_Libnx,LibnxError_OutOfMemory));return;}
    for(int group=0;group<4;group++) {
        int minimum=group==0?0:group==1?2:group==2?4:8;
        size_t used=snprintf(extra,60000,",\"options\":[");int selected=0;
        for(size_t i=0;i<count;i++)if(ext_games[i].players>=minimum) {
            char escaped[1100];ext_escape(ext_games[i].label,escaped,sizeof(escaped));
            if(used+strlen(escaped)+5>=60000)break;
            used+=snprintf(extra+used,60000-used,"%s\"%s\"",selected++?",":"",escaped);
        }
        snprintf(extra+used,60000-used,"]");
        char id[30],name[60];snprintf(id,sizeof(id),minimum?"game_%dp":"game",minimum);
        snprintf(name,sizeof(name),minimum?"Games for %d+ players":"Installed games",minimum);
        if(selected) {
            /* Catalogs can exceed MQTT_BUFFER; build this discovery packet on heap. */
            char topic[256],cmd[192],av[192],safe[80],suffix[64];
            discovery_topic(c,"select",id,topic,sizeof(topic));
            snprintf(suffix,sizeof(suffix),"control/%s",id);state_topic(c,suffix,cmd,sizeof(cmd));
            state_topic(c,"availability",av,sizeof(av));sanitize_id(client_id_or_device(c),safe,sizeof(safe));
            char *json=malloc(strlen(extra)+1300);
            if(json){snprintf(json,strlen(extra)+1300,"{\"name\":\"%s\",\"uniq_id\":\"%s_ext_%s\",\"cmd_t\":\"%s\",\"optimistic\":true,\"dev\":{\"ids\":[\"%s\"]}%s}",name,safe,id,cmd,safe,extra);ext_availability(c,id,json,strlen(extra)+1300);mqtt_send_publish(fd,topic,json,true);free(json);}
        } else clear_discovery_topic(fd,c,"select",id);
        char count_id[40];snprintf(count_id,sizeof(count_id),"%s_count",id);
        ext_entity(fd,c,"sensor",count_id,name,"",false);ext_number(fd,c,count_id,selected);
    }
    free(extra);ext_result(fd,c,"refresh_games",0);
}

static void ext_diagnostics(AppState*s,int fd,const AppConfig*c) {
    char logs[SHA_MAX_LOG][SHA_LOG_LINE];int count;
    mutexLock(&s->lock);
    count=s->log_count;memcpy(logs,s->log_lines,sizeof(logs));
    mutexUnlock(&s->lock);
    char json[MQTT_BUFFER];size_t used=snprintf(json,sizeof(json),"{\"build\":\"%s\",\"connections\":%llu,\"services\":{\"lbl\":%s,\"audctl\":%s,\"hid\":%s,\"pminfo\":%s},\"logs\":[",SHA_APP_BUILD,(unsigned long long)ext_connections,s->svc_lbl_ready?"true":"false",s->svc_audctl_ready?"true":"false",s->svc_hid_ready?"true":"false",s->svc_pminfo_ready?"true":"false");
    unsigned emitted=0;
    for(int i=0;i<count && i<SHA_MAX_LOG;i++) {
        /* Whitelist technical messages; omit notifications, names, hosts and tokens. */
        if(strncmp(logs[i],"MQTT connected",14)!=0 && strncmp(logs[i],"MQTT disconnected",17)!=0 && strncmp(logs[i],"MQTT rejected rc=",17)!=0 && strncmp(logs[i],"MQTT CONNECT send failed",24)!=0)continue;
        char escaped[SHA_LOG_LINE*6];ext_escape(logs[i],escaped,sizeof(escaped));
        if(used+strlen(escaped)+5>=sizeof(json))break;
        used+=snprintf(json+used,sizeof(json)-used,"%s\"%s\"",emitted++?",":"",escaped);
    }
    snprintf(json+used,sizeof(json)-used,"]}");
    char topic[192];state_topic(c,"diagnostic_attributes",topic,sizeof(topic));
    mqtt_send_publish(fd,topic,json,true);
    publish_value(fd,c,"diagnostics","reported");
}

static bool ext_reject_unsafe_command(int fd, const AppConfig *config, const char *command) {
    if (strcasecmp(command, "close_game") == 0 || strcasecmp(command, "close") == 0 || strcasecmp(command, "exit_game") == 0) {
        publish_value(fd, config, "command_status", "close_game:disabled_unsafe_termination");
        return true;
    }
    return false;
}

static bool extended_command(AppState*s,int fd,const AppConfig*c,const char*kind,const char*payload) {
    const char *cmd=strcmp(kind,"command")==0?payload:kind;
    if(ext_reject_unsafe_command(fd,c,cmd)) return true;
    Result rc=MAKERESULT(Module_Libnx,LibnxError_NotInitialized);
    if(strcmp(cmd,"stream_enabled")==0) {
        publish_value(fd,c,"command_status","stream_enabled:removed");
        return true;
    }
    else if(strcmp(cmd,"refresh")==0) { mutexLock(&s->lock);s->sensor_publish_requested=true;mutexUnlock(&s->lock);rc=0; }
    else if(strcmp(cmd,"get_logs")==0) {
        ext_diagnostics(s,fd,c);rc=0;
    } else if(strcmp(cmd,"set_brightness")==0 || strcmp(cmd,"screen")==0) {
        if(s->svc_lbl_ready) {
            if(strcmp(cmd,"screen")==0) {
                if(strcmp(payload,"ON")==0) rc=lblSwitchBacklightOn(0);
                else if(strcmp(payload,"OFF")==0) rc=lblSwitchBacklightOff(0);
            } else {
                char *end; float v=strtof(payload,&end);
                if(end!=payload && *end==0 && v>=0 && v<=100) {rc=lblSetCurrentBrightnessSetting(v/100);if(R_SUCCEEDED(rc))rc=lblApplyCurrentBrightnessSettingToBacklight();}
            }
        }
    } else if(strcmp(cmd,"set_volume")==0 || strcmp(cmd,"volume_up")==0 || strcmp(cmd,"volume_down")==0 || strcmp(cmd,"mute")==0) {
        if(s->svc_audctl_ready) {
            if(strcmp(cmd,"volume_up")==0) rc=audio_control_step(1);
            else if(strcmp(cmd,"volume_down")==0) rc=audio_control_step(-1);
            else if(strcmp(cmd,"mute")==0) {
                if(strcmp(payload,"ON")==0 || strcmp(payload,"OFF")==0)
                    rc=audio_control_set_mute(strcmp(payload,"ON")==0);
                else rc=MAKERESULT(Module_Libnx,LibnxError_BadInput);
            } else {
                char *end;float percent=strtof(payload,&end);
                rc=(end!=payload && *end==0)?audio_control_set_percent(percent):MAKERESULT(Module_Libnx,LibnxError_BadInput);
            }
        }
    } else if(strcmp(cmd,"refresh_games")==0) {ext_games_refresh(fd,c);return true;}
    else if(strcmp(cmd,"game")==0 || strcmp(cmd,"game_2p")==0 || strcmp(cmd,"game_4p")==0 || strcmp(cmd,"game_8p")==0) {
        for(size_t i=0;i<ext_game_count;i++)if(strcmp(payload,ext_games[i].label)==0) {
            if(s->app_mode==AppMode_ForegroundApp) rc=appletRequestLaunchApplication(ext_games[i].id,NULL);
            /* AppletType_None cannot launch through appletRequestLaunchApplication.
             * Keep the error/availability explicit until a background launch service exists. */
            break;
        }
    } else if(strcmp(cmd,"sleep")==0 || strcmp(cmd,"standby")==0) {
        /* Disabled after a console crash. Never invoke privileged sleep from MQTT. */
        publish_value(fd,c,"command_status","sleep:disabled");
        return true;
    } else return false;
    ext_result(fd,c,cmd,rc);
    ext_next_poll=0;
    mutexLock(&s->lock);s->sensor_publish_requested=true;mutexUnlock(&s->lock);
    return true;
}
