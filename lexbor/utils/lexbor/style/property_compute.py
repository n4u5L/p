import datetime
import os
import re
import sys


ABS_PATH = os.path.dirname(os.path.abspath(__file__))
sys.path.append("{}/../lexbor/".format(ABS_PATH))

import LXB


class PropertyCompute:
    tmp = "tmp/property_compute_res.h"
    source = "../../../source/lexbor/style/property_compute.h"
    property_const = "../../../source/lexbor/css/property/const.h"
    save_to = "../../../source/lexbor/style/property_compute_res.h"

    def make(self):
        entries = {}
        pattern = re.compile(
            r"^LXB_API\s+void\s+"
            r"(lxb_style_property_compute_([a-zA-Z0-9_]+))"
            r"\s*\(\s*void\s*\*\s*ctx\s*\)\s*;"
        )

        source = os.path.normpath(os.path.join(ABS_PATH, self.source))

        with open(source, "rt", encoding="utf-8") as fh:
            for line in fh:
                match = pattern.match(line.strip())
                if match is None:
                    continue

                func_name = match.group(1)
                property_name = self.make_property_enum(match.group(2))

                entries[property_name] = func_name

        properties = self.make_property_order()

        return [entries.get(name, "NULL") for name in properties]

    def make_property_order(self):
        properties = []
        pattern = re.compile(r"^\s*(LXB_CSS_PROPERTY_[A-Z0-9_]+)\s*=")
        source = os.path.normpath(os.path.join(ABS_PATH, self.property_const))

        with open(source, "rt", encoding="utf-8") as fh:
            for line in fh:
                match = pattern.match(line)
                if match is None:
                    continue

                name = match.group(1)
                if name == "LXB_CSS_PROPERTY__LAST_ENTRY":
                    break

                properties.append(name)

        return properties

    def make_property_enum(self, name):
        return "LXB_CSS_PROPERTY_{}".format(name.upper())

    def make_res(self, entries):
        result = []

        for func_name in entries:
            result.append("    {},".format(func_name))

        return "\n".join(result)

    def save(self):
        now = datetime.datetime.now()
        entries = self.make()

        tmp = os.path.normpath(os.path.join(ABS_PATH, self.tmp))
        save_to = os.path.normpath(os.path.join(ABS_PATH, self.save_to))

        lxb_temp = LXB.Temp(tmp, save_to)
        lxb_temp.pattern_append("%%YEAR%%", str(now.year))
        lxb_temp.pattern_append("%%BODY%%", self.make_res(entries))
        lxb_temp.build()
        lxb_temp.save()

        print("Res saved to {}".format(save_to))


if __name__ == "__main__":
    pc = PropertyCompute()
    pc.save()
