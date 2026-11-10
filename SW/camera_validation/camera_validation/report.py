"""Matching HTML and paginated PDF evidence reports."""
from collections import Counter
from html import escape
from pathlib import Path
import json
import hashlib

COLORS={'PASS':'#087f72','FAIL':'#be3544','SKIP':'#a96505','NOT_RUN':'#66768c','ERROR':'#be3544'}


def write_manifest(output):
    output=Path(output)
    files={str(f.relative_to(output)):hashlib.sha256(f.read_bytes()).hexdigest() for f in output.rglob('*') if f.is_file() and f.name!='sha256.json'}
    (output/'sha256.json').write_text(json.dumps(files,indent=2)+'\n')


def image_artifacts(value):
    if isinstance(value,dict):
        if {'name','path','array'} <= value.keys():yield value
        else:
            for child in value.values():yield from image_artifacts(child)
    elif isinstance(value,list):
        for child in value:yield from image_artifacts(child)


def render(document, output):
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    results=document['results'];counts=Counter(r['status'] for r in results)
    total=len(results);passed=counts['PASS']
    functional=[r for r in results if r['level'].startswith('functional-')]
    functional_pass=sum(r['status']=='PASS' for r in functional)
    qualification=f'Functional assertions: {functional_pass}/{len(functional)} passed. Supplemental diagnostics: {len(results)-len(functional)} cases. Case counts are not a percentage of working physical features.'
    cards=''.join(f'<div class="card"><strong style="color:{color}">{counts[status]}</strong>{status}</div>' for status,color in COLORS.items())
    rows=''.join(f'<tr><td>{escape(r["id"])}</td><td>{escape(r["feature"])}</td><td>{escape(r["level"])}</td><td style="color:{COLORS[r["status"]]}">{r["status"]}</td><td>{escape(r.get("detail",""))}</td></tr>' for r in results)
    def gallery(row):
        directory=row.get('artifact_directory','')
        return '<div class="gallery">'+''.join(
            f'<figure><a href="{escape(directory+"/"+item["array"],quote=True)}"><img src="{escape(directory+"/"+item["path"],quote=True)}" alt="{escape(item["name"],quote=True)}" loading="lazy"></a><figcaption>{escape(item["name"])}</figcaption></figure>'
            for item in image_artifacts(row.get('evidence',{})))+'</div>'
    details=''.join(f'<details><summary>{escape(r["id"])} - {r["status"]}</summary><p>{escape(r["purpose"])}</p><pre>{escape(json.dumps(r.get("evidence",{}),indent=2))}</pre><p>{escape(r.get("detail",""))}</p>{gallery(r)}</details>' for r in results)
    metadata=escape(json.dumps({k:v for k,v in document.items() if k!='results'},indent=2))
    html=f'''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>MaxCamera validation</title>
<style>body{{margin:0;background:#edf2f8;color:#182940;font:15px system-ui,sans-serif}}main{{max-width:1200px;margin:auto;padding:36px}}header{{background:#122742;color:white;padding:40px;border-radius:20px}}h1{{font-size:38px;margin:8px 0}}.eyebrow{{color:#74d7c8;letter-spacing:3px}}.cards{{display:flex;gap:16px;flex-wrap:wrap;margin:24px 0}}.card{{background:white;border-radius:12px;padding:20px;min-width:115px}}strong{{display:block;font-size:34px}}section{{background:white;padding:24px;border-radius:16px;margin:20px 0}}table{{width:100%;border-collapse:collapse;font-size:13px}}td,th{{text-align:left;padding:12px;border-bottom:1px solid #dae1ea;overflow-wrap:anywhere}}th{{background:#e8eff8}}pre{{white-space:pre-wrap;overflow-wrap:anywhere}}details{{padding:14px;border-bottom:1px solid #dae1ea}}summary{{cursor:pointer}}.gallery{{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:12px}}figure{{margin:0}}figure img{{width:100%;height:auto}}figcaption{{font-size:12px;padding:5px}}.bar{{height:10px;background:#dce4ef;border-radius:8px}}.bar span{{display:block;height:100%;background:#087f72;border-radius:8px}}@media print{{details{{display:block}}main{{padding:0}}}}</style>
<main><header><div class="eyebrow">MAXCAMERA / ETHERNET VALIDATION</div><h1>Camera validation report</h1><p>{escape(document['mode'])} | {escape(document['created'])}</p><p>Evidence through GVCP and GVSP. No Xilinx development tools.</p></header>
<div class="cards">{cards}</div><section><h2>Execution coverage</h2><p>{escape(qualification)}</p><p>{passed} of {total} cases passed. Skips and unexecuted cases are coverage gaps.</p><div class="bar"><span style="width:{100*passed/max(total,1):.1f}%"></span></div><p>Functional cases compare decoded pixels with an independent pattern, transformed baseline or pinned golden image. Response cases assert measured luminance, color or cadence. Flash cases deliberately poison the active state before loading and comparing profiles and actual images. Register diagnostics never establish functional coverage.</p></section>
<section><h2>Run context</h2><pre>{metadata}</pre></section><section><h2>Case matrix</h2><table><thead><tr><th>Case</th><th>Feature</th><th>Evidence level</th><th>Status</th><th>Detail</th></tr></thead><tbody>{rows}</tbody></table></section><section><h2>Evidence and assertions</h2>{details}</section></main></html>'''
    (output/'report.html').write_text(html,encoding='utf-8')
    from reportlab.lib import colors
    from reportlab.lib.styles import getSampleStyleSheet, ParagraphStyle
    from reportlab.lib.enums import TA_CENTER
    from reportlab.lib.pagesizes import A4
    from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, PageBreak, Image as PdfImage, KeepTogether
    styles=getSampleStyleSheet()
    styles.add(ParagraphStyle(name='CoverTitle',fontSize=32,leading=39,textColor=colors.HexColor('#122742'),spaceAfter=24))
    styles.add(ParagraphStyle(name='SmallCell',fontSize=8,leading=11,wordWrap='CJK'))
    styles.add(ParagraphStyle(name='Badge',fontSize=20,leading=25,alignment=TA_CENTER,textColor=colors.HexColor('#087f72')))
    p=lambda s,style='BodyText':Paragraph(escape(str(s)).replace('\n','<br/>'),styles[style])
    story=[Spacer(1,50),p('MAXCAMERA / ETHERNET VALIDATION','Heading2'),p('Camera validation report','CoverTitle'),p(document['mode'],'Heading2'),p(document['created']),Spacer(1,24)]
    summary=Table([[p(str(counts[s]),'Badge') for s in COLORS],[p(s,'SmallCell') for s in COLORS]],colWidths=[95]*5)
    summary.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,-1),colors.HexColor('#edf2f8')),('BOX',(0,0),(-1,-1),1,colors.HexColor('#dae1ea')),('TOPPADDING',(0,0),(-1,-1),12),('BOTTOMPADDING',(0,0),(-1,-1),12)]))
    story += [summary,Spacer(1,24),p('Evidence scope','Heading1'),p(qualification),p(f'{passed} / {total} cases passed. Skips and NOT_RUN are gaps, never passes.'),p('Functional PASS requires expected-pixel, measured-response or Flash persistence assertions. Stored profiles are tested by saving, deliberately changing active state, loading and comparing both settings and images. Baseline, expected, actual and difference images are retained. Register diagnostics do not establish functional coverage.'),p('Run context','Heading1')]
    for k,v in document.items():
        if k!='results':story.append(p(f'{k}: {v}','SmallCell'))
    story += [PageBreak(),p('Case matrix','Heading1')]
    table=[[p(s,'SmallCell') for s in ['Case / feature','Level','Status']]]
    for r in results:table.append([p(r['id']+' / '+r['feature'],'SmallCell'),p(r['level'],'SmallCell'),p(r['status'],'SmallCell')])
    matrix=Table(table,colWidths=[335,80,60],repeatRows=1,hAlign='LEFT')
    matrix.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),colors.HexColor('#dce8f5')),('ROWBACKGROUNDS',(0,1),(-1,-1),[colors.white,colors.HexColor('#f1f5fa')]),('VALIGN',(0,0),(-1,-1),'TOP'),('TOPPADDING',(0,0),(-1,-1),7),('BOTTOMPADDING',(0,0),(-1,-1),7)]))
    story += [matrix,PageBreak(),p('Individual case evidence','Heading1')]
    for r in results:
        story += [p(r['id'],'Heading3'),p(r['status']+' | '+r['level']),p(r['purpose']),p(r.get('detail',''),'SmallCell')]
        images=list(image_artifacts(r.get('evidence',{})))
        for start in range(0,len(images),4):
            cells=[];labels=[]
            for item in images[start:start+4]:
                path=output/r.get('artifact_directory','')/item['path']
                if not path.is_file():continue
                picture=PdfImage(str(path))
                ratio=min(112/picture.imageWidth,110/picture.imageHeight)
                picture.drawWidth=picture.imageWidth*ratio;picture.drawHeight=picture.imageHeight*ratio
                cells.append(picture);labels.append(p(item['name'],'SmallCell'))
            if cells:
                story.append(KeepTogether(Table([cells,labels],colWidths=[118]*len(cells))))
        evidence=json.dumps(r.get('evidence',{}),indent=2)
        # Keep evidence paragraphs bounded so large traces cannot create oversized rows.
        for line in evidence.splitlines():story.append(p(line,'SmallCell'))
        story.append(Spacer(1,9))
    def footer(canvas,doc):
        canvas.setTitle('MaxCamera Ethernet validation report')
        canvas.setAuthor('MaxCamera validation')
        canvas.setStrokeColor(colors.HexColor('#dce4ef'));canvas.line(40,38,555,38)
        canvas.setFont('Helvetica',8);canvas.drawString(40,25,'MAXCAMERA | GVCP / GVSP | '+document['mode']);canvas.drawRightString(555,25,str(doc.page))
    SimpleDocTemplate(str(output/'report.pdf'),pagesize=A4,rightMargin=40,leftMargin=40,topMargin=45,bottomMargin=50).build(story,onFirstPage=footer,onLaterPages=footer)
