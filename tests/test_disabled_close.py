"""Legacy close commands must report disabled without invoking process services."""
import pathlib
import subprocess
import tempfile
import unittest
from test_mqtt_discovery import ROOT, function


class DisabledCloseTest(unittest.TestCase):
    def test_legacy_aliases_only_publish_disabled_status(self):
        source = (ROOT / "source/mqtt_extensions.inc").read_text()
        code = r'''
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
typedef struct {int unused;} AppConfig;
static unsigned responses;
static void publish_value(int fd,const AppConfig*c,const char*key,const char*value){
 (void)fd;(void)c;
 assert(strcmp(key,"command_status")==0);
 assert(strcmp(value,"close_game:disabled_unsafe_termination")==0);
 responses++;
}
'''
        code += function(source, "ext_reject_unsafe_command")
        code += r'''
int main(void){
 AppConfig c={0};
 assert(ext_reject_unsafe_command(1,&c,"close_game"));
 assert(ext_reject_unsafe_command(1,&c,"close"));
 assert(ext_reject_unsafe_command(1,&c,"exit_game"));
 assert(ext_reject_unsafe_command(1,&c,"CLOSE_GAME"));
 assert(!ext_reject_unsafe_command(1,&c,"refresh"));
 assert(responses==4);
 puts("Legacy close commands: disabled without process termination");
}
'''
        with tempfile.TemporaryDirectory() as temp:
            c = pathlib.Path(temp) / "disabled.c"
            binary = pathlib.Path(temp) / "disabled"
            c.write_text(code)
            subprocess.run(["gcc", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined", str(c), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
