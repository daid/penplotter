from xml.etree import ElementTree
import os
import re


def path_to_lines(unicode, path):
    if path is None:
        return []
    # print(path)
    result = []
    current = None
    for m in re.finditer(r"([A-Za-z])([^A-Za-z]*)", path):
        cmd, params = m.groups()
        params = tuple(float(p) for p in params.replace(",", " ").split())
        if cmd == "M":
            assert len(params) == 2
            current = [params]
            result.append(current)
        elif cmd == "L":
            assert len(params) == 2
            current.append(params)
        else:
            raise RuntimeError(f"Unsupported SVG path command: {cmd} {params} ({unicode})")
    return result


def process(filename):
    print(filename)
    root = ElementTree.parse(filename).getroot()
    glyphs = {}
    for e in root.iter("{http://www.w3.org/2000/svg}font-face"):
        pass # print(e.attrib['ascent'])
    for e in root.iter("{http://www.w3.org/2000/svg}glyph"):
        unicode = e.attrib.get('unicode')
        path = e.attrib.get('d')
        if unicode is None or ord(unicode) > 128:
            continue
        glyphs[unicode] = (float(e.attrib.get('horiz-adv-x')), path_to_lines(unicode, path))
    return glyphs


class Dumper:
    def __init__(self, filename):
        self.__f = open(filename, "wt")
        self.__f.write('<?xml version="1.0" standalone="no"?><svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 2000 1100">\n')
        self.__f.write('<path d="')
        self.__scale = 50
        self.__offset_y = 1250 / self.__scale

    def dump(self, glyphs, characters=None):
        offset = 0
        SCALE = 50
        if characters is None:
            characters = glyphs.keys()
        for character in characters:
            advance, lines = glyphs[character]
            for line in lines:
                for idx, (x, y) in enumerate(line):
                    self.__f.write("M" if idx == 0 else "L")
                    self.__f.write(f'{x/SCALE+offset} {self.__offset_y-y/SCALE} ')
            offset += advance/SCALE
        self.__offset_y += 1250 / self.__scale

    def __del__(self):
        self.__f.write('" fill="transparent" stroke="#000" stroke-width=".05em" stroke-linecap="round" stroke-linejoin="round"/>')
        self.__f.write('</svg>')


class Exporter:
    def __init__(self, filename):
        self.__all_fonts = []
        self.__f = open(filename, "wt")
        self.__f.write("""#include <stdint.h>

static constexpr int16_t _END_OF_LINE = 0x7FFF;

struct Glyph {
    uint16_t codepoint;
    uint16_t advance;
    const int16_t* lines;
};
struct Font {
    const char* name;
    const Glyph* glyphs;
};
""")

    def store(self, name: str, glyphs):
        symbol = name.lower()
        for unicode, (advance, lines) in glyphs.items():
            self.__f.write(f"static const int16_t _font_glyph_{symbol}_{ord(unicode)}[] = {{")
            self.__f.write(f"{int(advance)},")
            for line in lines:
                for x, y in line:
                    self.__f.write(f"{int(x)},{int(y)},")
                self.__f.write(f"_END_OF_LINE,")
            self.__f.write(f"_END_OF_LINE,")
            self.__f.write(f"}};\n")
        self.__f.write(f"static const Glyph _font_glyphs_{symbol}[] = {{\n")
        for unicode, (advance, lines) in glyphs.items():
            self.__f.write(f"  {{{ord(unicode)}, {int(advance)}, _font_glyph_{symbol}_{ord(unicode)}}},\n")
        self.__f.write(f"  {{0, 0, nullptr}},\n")
        self.__f.write(f"}};\n")
        self.__f.write(f"static const Font _font_{symbol} = {{\"{name}\", _font_glyphs_{symbol}}};\n")
        self.__all_fonts.append(symbol)

    def __del__(self):
        self.__f.write(f"static const Font* _all_fonts[] = {{\n")
        for symbol in self.__all_fonts:
            self.__f.write(f"  &_font_{symbol},\n")
        self.__f.write(f"  nullptr,\n")
        self.__f.write(f"}};\n")


def main():
    # d = Dumper("dump.svg")
    e = Exporter("fonts.cpp")
    for path, dirs, files in os.walk("svg-fonts/fonts"):
        for file in files:
            if file.endswith(".svg"):
                if file == "TwinSans.svg":  # Skip this font which has curves in the ascii set
                    continue
                glyphs = process(os.path.join(path, file))
                # d.dump(glyphs, file)
                e.store(os.path.splitext(file)[0], glyphs)


if __name__ == "__main__":
    main()
