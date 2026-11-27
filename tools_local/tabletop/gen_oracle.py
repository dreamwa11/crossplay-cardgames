#!/usr/bin/env python3
"""Pick the oracle tables out of Datasworn and write them for the device and the web.

    python3 tools_local/tabletop/gen_oracle.py <datasworn checkout>

Datasworn (github.com/rsek/datasworn) carries the Ironsworn, Ironsworn: Delve
and Ironsworn: Starforged oracle tables as JSON. Their text is by Shawn Tomkin,
used under CC BY 4.0. Writes:

    src/apps_local/tabletop/OracleTables.h   (the device)
    tools_local/tabletop/oracle_tables.js    (pasted into the web Tabletop)

Each table keeps its die weights: `cum` holds each row's upper bound on a d100
(or 1..n for a table that merges two sources). Rows that point at other tables
("Roll twice", links, "Check the theme card") are dropped, and the text is
folded to ASCII so the device fonts have every glyph.
"""
import json
import re
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# name -> list of (book, table id); one source keeps its weights, two merge flat.
PICK = {
    # Fantasy: Ironsworn and Delve.
    "action": [("classic", "action_and_theme/action")],
    "theme": [("classic", "action_and_theme/theme")],
    "aspect": [("delve", "feature/aspect")],
    "feature": [("delve", "feature/focus")],
    "name_a": [("classic", "name/ironlander/a")],
    "name_b": [("classic", "name/ironlander/b")],
    "name_elf": [("classic", "name/elf")],
    "trait": [("classic", "character/descriptor")],
    "role": [("classic", "character/role")],
    "goal": [("classic", "character/goal")],
    "activity": [("delve", "character/activity")],
    "disposition": [("delve", "character/disposition")],
    "place_desc": [("classic", "place/descriptor")],
    "location": [("classic", "place/location")],
    "danger": [("delve", "moves/reveal_a_danger"), ("classic", "moves/pay_the_price")],
    "chance": [("delve", "moves/find_an_opportunity")],
    # Sci-fi: Starforged.
    "sf_action": [("starforged", "core/action")],
    "sf_theme": [("starforged", "core/theme")],
    "descriptor": [("starforged", "core/descriptor")],
    "focus": [("starforged", "core/focus")],
    "sf_given": [("starforged", "characters/name/given")],
    "sf_family": [("starforged", "characters/name/family_name")],
    "sf_callsign": [("starforged", "characters/name/callsign")],
    "sf_look": [("starforged", "characters/first_look")],
    "sf_role": [("starforged", "characters/role")],
    "sf_goal": [("starforged", "characters/goal")],
    "sf_disposition": [("starforged", "characters/initial_disposition")],
    "set_name": [("starforged", "settlements/name")],
    "set_where": [("starforged", "settlements/location")],
    "set_look": [("starforged", "settlements/first_look")],
    "set_trouble": [("starforged", "settlements/trouble")],
    "planet_class": [("starforged", "planets/class")],
    "ship_name": [("starforged", "starships/starship_name")],
    "ship_type": [("starforged", "starships/type")],
    "ship_look": [("starforged", "starships/first_look")],
    "ship_contact": [("starforged", "starships/initial_contact")],
    "sf_danger": [("starforged", "misc/story_complication"), ("starforged", "space/peril")],
    "sf_chance": [("starforged", "space/opportunity")],
    # Both.
    "twist": [("classic", "turning_point/major_plot_twist")],
    "clue": [("starforged", "misc/story_clue")],
    "fight": [("starforged", "misc/combat_action")],
}

# Tables made of one table per planet class, kept as one list whose `group`
# says which class each row belongs to (the class's row in planet_class).
PLANETS = ["desert", "furnace", "grave", "ice", "jovian", "jungle", "ocean", "rocky", "shattered", "tainted", "vital"]
GROUPED = {"planet_name": "name", "planet_view": "observed_from_space"}

DROP = ("roll twice", "roll again", "check the", "unusual role", "[", "](")

# Actions that do not take an object on their own, so a sentence can read
# "someone moves to scheme against the ally".
VERB = {
    "Scheme": "scheme against", "Clash": "clash with", "Swear": "swear by", "Falter": "falter over",
    "Focus": "focus on", "Persevere": "persevere with", "Arrive": "arrive at", "Journey": "journey toward",
    "Communicate": "communicate with", "Depart": "depart from", "Search": "search for", "Withdraw": "withdraw from",
}

FOLD = {"’": "'", "‘": "'", "“": '"', "”": '"', "—": "-", "–": "-", "…": "..."}


def find(node, suffix):
    if isinstance(node, dict):
        if str(node.get("_id", "")).endswith(suffix) and "rows" in node:
            return node
        for v in node.values():
            hit = find(v, suffix)
            if hit:
                return hit
    elif isinstance(node, list):
        for v in node:
            hit = find(v, suffix)
            if hit:
                return hit
    return None


def clean(text):
    # A row that is only a link to another collection keeps the link's words:
    # "[Desert World](id:...)" is "Desert World".
    m = re.fullmatch(r"\[([^\]]+)\]\(id:[^)]*\)", text.strip())
    if m:
        text = m.group(1)
    for a, b in FOLD.items():
        text = text.replace(a, b)
    text = text.strip()
    if text.endswith("."):
        text = text[:-1]
    if any(ord(c) > 126 for c in text):
        raise SystemExit(f"non-ASCII left in {text!r}")
    return text


def main():
    src = sys.argv[1]
    books = {b: json.load(open(os.path.join(src, "datasworn", b, f"{b}.json"))) for b in ("classic", "delve", "starforged")}
    tables = {}
    for name, sources in PICK.items():
        rows = []
        for book, tid in sources:
            t = find(books[book]["oracles"], f"{book}/oracles/{tid}")
            if not t:
                raise SystemExit(f"missing {book}/{tid}")
            for r in t["rows"]:
                text = r.get("text") or ""
                if name == "planet_class":
                    text = clean(text)
                # "It is harmful" leans on a move's own wording; alone it says nothing.
                if not text or any(d in text.lower() for d in DROP) or text.startswith("It "):
                    continue
                weight = (r["max"] - r["min"] + 1) if len(sources) == 1 and r.get("min") is not None else 1
                rows.append((clean(text), weight))
        cum, run = [], 0
        for _, w in rows:
            run += w
            cum.append(run)
        tables[name] = {"text": [t for t, _ in rows], "cum": cum}
    for verb, act in (("verb", "action"), ("sf_verb", "sf_action")):
        a = tables[act]
        tables[verb] = {"text": [VERB.get(t, t.lower()) for t in a["text"]], "cum": a["cum"]}
    if [t.split()[0].lower() for t in tables["planet_class"]["text"]] != PLANETS:
        raise SystemExit(f"planet classes moved: {tables['planet_class']['text']}")
    for name, tid in GROUPED.items():
        text, group = [], []
        for g, planet in enumerate(PLANETS):
            t = find(books["starforged"]["oracles"], f"starforged/oracles/planets/{planet}/{tid}")
            for r in t["rows"]:
                row = r.get("text") or ""
                if row and not any(d in row.lower() for d in DROP):
                    text.append(clean(row))
                    group.append(g)
        tables[name] = {"text": text, "cum": list(range(1, len(text) + 1)), "group": group}

    # The device.
    h = [
        "#pragma once",
        "",
        "// Generated by tools_local/tabletop/gen_oracle.py from Datasworn. Do not edit.",
        "// Oracle tables from Ironsworn, Ironsworn: Delve and Ironsworn: Starforged",
        "// by Shawn Tomkin, used under CC BY 4.0 (creativecommons.org/licenses/by/4.0).",
        "",
        "#include <cstdint>",
        "",
        "namespace tabletop::oracle_tables {",
        "",
        "struct List {",
        "  const char* const* text;",
        "  const uint8_t* cum;  // each row's upper bound; the last is the die",
        "  uint8_t size;",
        "  const uint8_t* group = nullptr;  // planet class per row, for the grouped tables",
        "};",
        "",
    ]
    for name, t in tables.items():
        h.append(f"inline constexpr const char* k_{name}_text[] = {{")
        for s in t["text"]:
            h.append("    " + json.dumps(s) + ",")
        h.append("};")
        h.append(f"inline constexpr uint8_t k_{name}_cum[] = {{" + ", ".join(str(c) for c in t["cum"]) + "};")
        grp = "nullptr"
        if "group" in t:
            h.append(f"inline constexpr uint8_t k_{name}_group[] = {{" + ", ".join(str(g) for g in t["group"]) + "};")
            grp = f"k_{name}_group"
        if len(t["text"]) > 255:
            raise SystemExit(f"{name} has {len(t['text'])} rows; picks are bytes")
        h.append(f"inline constexpr List k{''.join(p.capitalize() for p in name.split('_'))} = {{k_{name}_text, k_{name}_cum, {len(t['text'])}, {grp}}};")
        h.append("")
    h.append("}  // namespace tabletop::oracle_tables")
    with open(os.path.join(ROOT, "src/apps_local/tabletop/OracleTables.h"), "w") as f:
        f.write("\n".join(h) + "\n")

    # The web.
    with open(os.path.join(ROOT, "tools_local/tabletop/oracle_tables.js"), "w") as f:
        f.write("// Generated by tools_local/tabletop/gen_oracle.py. Oracle tables from Ironsworn,\n")
        f.write("// Ironsworn: Delve and Ironsworn: Starforged by Shawn Tomkin, CC BY 4.0.\n")
        f.write("const ORACLE_TABLES = " + json.dumps(tables, separators=(",", ":")) + ";\n")
    total = sum(len(t["text"]) for t in tables.values())
    chars = sum(len(s) + 1 for t in tables.values() for s in t["text"])
    print(f"{len(tables)} tables, {total} rows, {chars} bytes of text")
    for n, t in tables.items():
        print(f"  {n:14} {len(t['text']):3} rows, die {t['cum'][-1]}")


if __name__ == "__main__":
    main()
