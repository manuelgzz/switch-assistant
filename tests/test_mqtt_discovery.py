"""Exercise production discovery builders on the host, without Switch services."""
import json
import pathlib
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def function(source, name):
    # Locate a definition rather than its forward declaration.
    import re
    match = re.search(r"^static [^\n]*\b" + name + r"\([^;]*?\)\s*\{", source, re.M)
    if not match:
        raise AssertionError(name)
    end = source.index("\n}", match.start()) + 2
    return source[match.start():end]


class DiscoveryTest(unittest.TestCase):
    def test_discovery_is_valid_and_device_scoped(self):
        main = (ROOT / "source/mqtt_client.c").read_text()
        ext = (ROOT / "source/mqtt_extensions.inc").read_text()
        prelude = r'''
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#define MQTT_BUFFER 2048
#define SHA_APP_BUILD "test"
typedef struct {char device_name[64],mqtt_client_id[64],mqtt_topic_prefix[128];} AppConfig;
typedef struct {int unused;} AppState;
static unsigned long long ext_connections,ext_session_started,ext_next_poll;
static unsigned long long monotonic_ms(void){return 1000;}
static void mqtt_send_publish(int fd,const char*topic,const char*payload,bool retain) {
 (void)fd;(void)retain;printf("%s\t%s\n",topic,payload);
}
'''
        prelude += "\nstatic void clear_discovery_topic(int fd,const AppConfig*c,const char*type,const char*id);\n"
        names = ["sanitize_id", "client_id_or_device", "state_topic", "discovery_topic", "clear_discovery_topic"]
        code = prelude + "\n".join(function(main, n) for n in names)
        code += "\n" + "\n".join(function(ext, n) for n in ["ext_escape", "ext_capability", "ext_availability", "ext_entity", "extended_discovery"])
        code += "\n" + function(main, "publish_discovery_sensor")
        code += r'''
int main(void) {
 AppConfig a={.device_name="Switch \"living\\room\"",.mqtt_client_id="switch-a",.mqtt_topic_prefix="homeassistant"};
 AppConfig b=a;strcpy(b.mqtt_client_id,"switch-b");
 ext_entity(1,&a,"number","set_brightness","Brightness",",\"min\":0,\"max\":100,\"step\":1",true);
 ext_entity(1,&b,"number","set_brightness","Brightness",",\"min\":0,\"max\":100,\"step\":1",true);
 ext_entity(1,&a,"sensor","process_memory","Memory",",\"unit_of_meas\":\"MiB\"",false);
 ext_entity(1,&a,"event","overlay_action","Actions",",\"event_types\":[\"quick_action_1\"]",false);
 AppState state={0};extended_discovery(&state,1,&a);
 publish_discovery_sensor(1,&a,"sensor","current_game","Current game",NULL,NULL,NULL);
 publish_discovery_sensor(1,&a,"sensor","current_game_id","Title ID",NULL,NULL,NULL);
 publish_discovery_sensor(1,&a,"binary_sensor","game_running","Game running","running",NULL,NULL);
 publish_discovery_sensor(1,&a,"sensor","operation_mode","Operation Mode",NULL,NULL,NULL);
}
'''
        with tempfile.TemporaryDirectory() as temp:
            c = pathlib.Path(temp) / "test.c"
            binary = pathlib.Path(temp) / "test"
            c.write_text(code)
            subprocess.run(["gcc", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined", str(c), "-o", str(binary)], check=True)
            result = subprocess.run([str(binary)], check=True, capture_output=True, text=True)
        packets = []
        removed = []
        for line in result.stdout.splitlines():
            topic, payload = line.split("\t", 1)
            if not payload:
                removed.append(topic)
                continue
            # Duplicate fields hide malformed availability declarations.
            def unique(pairs):
                output = {}
                for key, value in pairs:
                    self.assertNotIn(key, output)
                    output[key] = value
                return output
            packets.append((topic, json.loads(payload, object_pairs_hook=unique)))
        self.assertGreater(len(packets), 40)
        self.assertIn("homeassistant/button/switch_a/sleep/config", removed)
        self.assertIn("homeassistant/button/switch_a/close_game/config", removed)
        self.assertIn("homeassistant/switch/switch_a/stream_enabled/config", removed)
        a, b = packets[0][1], packets[1][1]
        self.assertNotEqual(a["cmd_t"], b["cmd_t"])
        self.assertNotEqual(a["uniq_id"], b["uniq_id"])
        self.assertEqual(a["cmd_t"], "switch_ha/switch_a/control/set_brightness")
        self.assertEqual(a["avty_mode"], "all")
        self.assertEqual(a["avty"][0]["topic"], "switch_ha/switch_a/status")
        self.assertEqual(a["avty"][0]["value_template"], "{{ value_json.status }}")
        self.assertEqual(a["avty"][1]["topic"], "switch_ha/switch_a/capability/lbl")
        self.assertEqual(a["dev"]["name"], 'Switch "living\\room"')
        self.assertEqual(packets[2][1]["unit_of_meas"], "MiB")
        self.assertEqual(packets[3][1]["event_types"], ["quick_action_1"])
        self.assertNotIn("cmd_t", packets[2][1])
        unique_ids = set()
        for topic, entity in packets[4:]:
            self.assertNotIn(entity["uniq_id"], unique_ids)
            unique_ids.add(entity["uniq_id"])
            if "/button/" in topic:
                self.assertNotIn("stat_t", entity)
                self.assertEqual(entity["payload_press"], "PRESS")
                self.assertTrue(entity["cmd_t"].startswith("switch_ha/switch_a/control/"))
            self.assertNotIn("daybreak", topic)
            self.assertNotIn("/sleep/", topic)
            self.assertNotIn("/close_game/", topic)
            self.assertNotIn("/stream_enabled/", topic)
        screens = [entity for topic, entity in packets if topic == "homeassistant/switch/switch_a/screen/config"]
        self.assertEqual(len(screens), 1)
        self.assertEqual(screens[0]["name"], "Screen")
        self.assertEqual(screens[0]["uniq_id"], "switch_a_ext_screen")
        telemetry_count = 0
        for topic, entity in packets:
            if "/sensor/" in topic or "/binary_sensor/" in topic:
                telemetry_count += 1
                self.assertNotIn("avty_t", entity)
                self.assertNotIn("avty", entity)
                self.assertNotIn("expire_after", entity)
        self.assertGreater(telemetry_count, 30)
        self.assertEqual(packets[-1][1]["stat_t"], "switch_ha/switch_a/status")
        self.assertEqual(packets[-1][1]["value_template"], "{{ value_json.operation_mode | default('unknown') }}")
        print(f"Validated {len(packets)} discovery payloads")


if __name__ == "__main__":
    unittest.main()
