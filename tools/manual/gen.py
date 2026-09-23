# -*- coding: utf-8 -*-
import os,re,sys
HERE=os.path.dirname(os.path.abspath(__file__))
ROOT=os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0,HERE)
import sections,tooltips,columns
def raw(t):
    assert ')MAN"' not in t
    # MSVC: einzelne Literale < 16 KB; Texte bleiben darunter, lange werden in Teile zerlegt
    parts=[t[i:i+6000] for i in range(0,len(t),6000)] or [""]
    return "\n    ".join('R"MAN('+p+')MAN"' for p in parts)
def cs(t): return '"'+t.replace('\\','\\\\').replace('"','\\"').replace('\n','\\n')+'"'
out=[]
out.append('// GENERIERT (Handbuch-Daten, zweisprachig) - siehe docs/MANUAL_MAINTENANCE.md zum Erweitern.\n#include "mapeditor/core/Manual.hpp"\n\nnamespace theseed::mapeditor::core::manual {\nnamespace {\n')
out.append('const Chapter kChapters[] = {')
for cid,de,en in sections.CHAPTERS: out.append(f'    {{{cs(cid)}, {cs(de)}, {cs(en)}}},')
out.append('};\n')
out.append('const Section kSections[] = {')
for sid,ch,tde,ten,bde,ben,kw in sections.SECTIONS:
    out.append(f'    {{{cs(sid)}, {cs(ch)},\n     {cs(tde)}, {cs(ten)},\n     {raw(bde)},\n     {raw(ben)},\n     {cs(kw)}}},')
out.append('};\n')
out.append('struct TipRow { const char* key; const char* de; const char* en; };\nconst TipRow kTips[] = {')
for k,(de,en) in tooltips.TIPS.items(): out.append(f'    {{{cs(k)}, {cs(de)}, {cs(en)}}},')
out.append('};\n')
# Spalten: manuell + aus main.cpp (Skill-Tabellen)
rows=[(t,c,cert,de,en) for (t,c,cert,de,en) in columns.COLS]
src=open(os.path.join(ROOT,'src/app/main.cpp'),encoding='utf-8').read()
a=src.index('namespace skilled {'); b=src.index('} // namespace skilled')
pat=re.compile(r'\{Doc::(Skill|Server|View), "([^"]+)", Kind::\w+, "((?:[^"\\]|\\.)*)", "((?:[^"\\]|\\.)*)", "((?:[^"\\]|\\.)*)", "((?:[^"\\]|\\.)*)"\}')
tab={'Skill':'ActiveSkill','Server':'ActiveSkillInfoServer','View':'ActiveSkillView'}
n=0
for m in pat.finditer(src[a:b]):
    doc,col,lde,len_,tde,ten=m.groups()
    cert='(vermutet)' not in tde and 'vermutet' not in tde and 'Aufzählung' not in tde
    rows.append((tab[doc],col,cert,tde.replace('\\"','"'),ten.replace('\\"','"'))); n+=1
print('Skill-Spalten aus main.cpp:',n)
out.append('const ColumnDoc kColumns[] = {')
for t,c,cert,de,en in rows: out.append(f'    {{{cs(t)}, {cs(c)}, {"true" if cert else "false"}, {cs(de)}, {cs(en)}}},')
out.append('};\n\n} // namespace\n')
out.append('const Chapter* ChapterData(std::size_t& n) { n = sizeof(kChapters) / sizeof(kChapters[0]); return kChapters; }\nconst Section* SectionData(std::size_t& n) { n = sizeof(kSections) / sizeof(kSections[0]); return kSections; }\nconst ColumnDoc* ColumnData(std::size_t& n) { n = sizeof(kColumns) / sizeof(kColumns[0]); return kColumns; }\nconst char* const* TipData(std::size_t& n) { (void)n; return nullptr; }\nvoid ForEachTip(void (*fn)(const char*, const char*, const char*, void*), void* user) { for (const auto& t : kTips) fn(t.key, t.de, t.en, user); }\n\n} // namespace theseed::mapeditor::core::manual\n')
open(os.path.join(ROOT,'src/core/ManualData.cpp'),'w',encoding='utf-8').write('\n'.join(out))
print(len(sections.SECTIONS),'Abschnitte',len(tooltips.TIPS),'Tooltips',len(rows),'Spalten')
