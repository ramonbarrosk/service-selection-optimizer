#!/usr/bin/env python3
"""Relatório PDF: GLS × ILS do artigo (10 s e 0,55 s) e ablação da GLS (0,55 s).

Requer reportlab (pip install reportlab). Gera docs/relatorio_gls_ils_ablacao.pdf.

    python3 experiments/08_ablacao_gls/scripts/plot_gls_ablation.py 0.55   # gráficos da ablação
    python3 experiments/08_ablacao_gls/scripts/build_report_pdf.py
"""

import csv
import re
import sys
from datetime import date
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import cm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (Image, PageBreak, Paragraph, SimpleDocTemplate, Spacer, Table,
                                TableStyle)

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experiments/08_ablacao_gls/scripts"))
import plot_gls_ablation as ablation  # noqa: E402

OUTPUT = ROOT / "docs/relatorio_gls_ils_ablacao.pdf"
CHARTS = ROOT / "experiments/08_ablacao_gls/charts/gls_ablation_0.55s"
RUNS = {
    "10": {
        "ils1": ROOT / "experiments/00_baseline_java/results/java_results_raw.txt",
        "ils3": ROOT / "experiments/00_baseline_java/results/java_ils3_10s_raw.txt",
        "gls": ROOT / "experiments/07_protocolo_artigo/results/gls_promoted_article_protocol.csv",
    },
    "0.55": {
        "ils1": ROOT / "experiments/00_baseline_java/results/java_ils1_0.55s_raw.txt",
        "ils3": ROOT / "experiments/00_baseline_java/results/java_ils3_0.55s_raw.txt",
        "gls": ROOT / "experiments/08_ablacao_gls/results/ablation_0.55s/i1_s1_l0_r1.csv",
    },
}

FONT_DIR = Path("/usr/share/fonts/truetype/dejavu")
pdfmetrics.registerFont(TTFont("DejaVu", str(FONT_DIR / "DejaVuSans.ttf")))
pdfmetrics.registerFont(TTFont("DejaVu-Bold", str(FONT_DIR / "DejaVuSans-Bold.ttf")))
pdfmetrics.registerFont(TTFont("DejaVu-Oblique", str(FONT_DIR / "DejaVuSans-Oblique.ttf")))
pdfmetrics.registerFontFamily("DejaVu", normal="DejaVu", bold="DejaVu-Bold",
                              italic="DejaVu-Oblique", boldItalic="DejaVu-Bold")

INK = colors.HexColor("#0b0b0b")
INK_2 = colors.HexColor("#52514e")
HEADER = colors.HexColor("#172554")
GRID = colors.HexColor("#e1e0d9")
ZEBRA = colors.HexColor("#f7f7f5")
HIGHLIGHT = colors.HexColor("#e8f1fc")
GAP_RAMP = ["#ffffff", "#cde2fb", "#b7d3f6", "#9ec5f4", "#86b6ef", "#6da7ec", "#5598e7"]

STYLES = {
    "title": ParagraphStyle("title", fontName="DejaVu-Bold", fontSize=19, leading=24,
                            textColor=INK, spaceAfter=4),
    "subtitle": ParagraphStyle("subtitle", fontName="DejaVu", fontSize=10.5, leading=14,
                               textColor=INK_2, spaceAfter=14),
    "h1": ParagraphStyle("h1", fontName="DejaVu-Bold", fontSize=14, leading=18,
                         textColor=INK, spaceBefore=14, spaceAfter=6),
    "h2": ParagraphStyle("h2", fontName="DejaVu-Bold", fontSize=11, leading=14,
                         textColor=INK, spaceBefore=10, spaceAfter=4),
    "body": ParagraphStyle("body", fontName="DejaVu", fontSize=9.5, leading=13.5,
                           textColor=INK, spaceAfter=5),
    "bullet": ParagraphStyle("bullet", fontName="DejaVu", fontSize=9.5, leading=13.5,
                             textColor=INK, leftIndent=12, bulletIndent=2, spaceAfter=3),
    "note": ParagraphStyle("note", fontName="DejaVu", fontSize=8, leading=11,
                           textColor=INK_2, spaceAfter=6),
    "cell": ParagraphStyle("cell", fontName="DejaVu", fontSize=8.5, leading=10.5,
                           textColor=INK),
    "cell_center": ParagraphStyle("cell_center", fontName="DejaVu", fontSize=8.5, leading=10.5,
                                  textColor=INK, alignment=TA_CENTER),
    "head": ParagraphStyle("head", fontName="DejaVu-Bold", fontSize=8.5, leading=10.5,
                           textColor=colors.white, alignment=TA_CENTER),
}

JAVA_ROW = re.compile(
    r"^Instance_10_10_(\d+)\s+(\d+)\s+([0-9.]+)\s+([0-9.]+)\s+([0-9.]+)\s+([0-9.]+)")


def fmt(value, digits=2):
    return f"{value:.{digits}f}".replace(".", ",")


def gap(cost, optimum):
    return 100.0 * (cost - optimum) / optimum


def read_java(path):
    rows = {}
    for line in path.read_text().splitlines():
        match = JAVA_ROW.match(line.strip())
        if match:
            iid, opt, _, mean, best, t = match.groups()
            rows[int(iid)] = dict(opt=int(opt), mean=float(mean), best=float(best), t=float(t))
    return rows


def read_gls(path):
    grouped = {}
    with path.open(newline="", encoding="utf-8") as source:
        for row in csv.DictReader(source):
            grouped.setdefault(int(row["instance"]), []).append(row)
    result = {}
    for iid, values in grouped.items():
        costs = [float(v["cost"]) for v in values]
        result[iid] = dict(
            opt=int(float(values[0]["optimum"])), mean=sum(costs) / len(costs), best=min(costs),
            t=sum(float(v["time_ms"]) for v in values) / len(values) / 1000,
            optimal_runs=sum(float(v["gap_pct"]) <= 1e-9 for v in values), runs=len(values))
    return result


def summarize(rows):
    ids = sorted(rows)
    n = len(ids)
    return dict(
        reached=sum(rows[i]["best"] <= rows[i]["opt"] for i in ids),
        mean=sum(rows[i]["mean"] for i in ids) / n,
        gap=sum(gap(rows[i]["mean"], rows[i]["opt"]) for i in ids) / n,
        t=sum(rows[i]["t"] for i in ids) / n,
        max_t=max(rows[i]["t"] for i in ids),
        n=n,
    )


def load_runs():
    data = {}
    for budget, paths in RUNS.items():
        data[budget] = {name: (read_java(p) if name != "gls" else read_gls(p))
                        for name, p in paths.items()}
    return data


def p(text, style="body"):
    return Paragraph(text, STYLES[style])


def bullets(items):
    return [Paragraph(item, STYLES["bullet"], bulletText="•") for item in items]


GAP_BANDS = [(1e-9, None), (1.0, "#cde2fb"), (5.0, "#9ec5f4"), (20.0, "#6da7ec"),
             (float("inf"), "#3d86dc")]


def band_color(value):
    for limit, color in GAP_BANDS:
        if value <= limit:
            return color
    return GAP_BANDS[-1][1]


def table(header, rows, widths, highlight_rows=(), align_left_cols=(0,), gap_cols=None,
          font_size=None, banded=False):
    cell = STYLES["cell"]
    center = STYLES["cell_center"]
    head = STYLES["head"]
    if font_size:
        cell = ParagraphStyle("c", parent=cell, fontSize=font_size, leading=font_size + 2)
        center = ParagraphStyle("cc", parent=center, fontSize=font_size, leading=font_size + 2)
        head = ParagraphStyle("h", parent=head, fontSize=font_size, leading=font_size + 2)
    data = [[Paragraph(h, head) for h in header]]
    for row in rows:
        data.append([Paragraph(str(v), cell if c in align_left_cols else center)
                     for c, v in enumerate(row)])
    style = [
        ("BACKGROUND", (0, 0), (-1, 0), HEADER),
        ("GRID", (0, 0), (-1, -1), 0.5, GRID),
        ("VALIGN", (0, 0), (-1, -1), "MIDDLE"),
        ("TOPPADDING", (0, 0), (-1, -1), 3),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
    ]
    for r in range(1, len(data)):
        if r % 2 == 0:
            style.append(("BACKGROUND", (0, r), (-1, r), ZEBRA))
    for r in highlight_rows:
        style.append(("BACKGROUND", (0, r + 1), (-1, r + 1), HIGHLIGHT))
    if gap_cols:
        for col, values in gap_cols.items():
            top = max(values) or 1
            for r, v in enumerate(values, 1):
                if banded:
                    color = band_color(v)
                else:
                    step = round(min(v, top) / top * (len(GAP_RAMP) - 1))
                    color = GAP_RAMP[step] if step else None
                if color:
                    style.append(("BACKGROUND", (col, r), (col, r), colors.HexColor(color)))
    t = Table(data, colWidths=widths, repeatRows=1, hAlign="LEFT")
    t.setStyle(TableStyle(style))
    return t


def figure(path, width_cm):
    from reportlab.lib.utils import ImageReader
    w, h = ImageReader(str(path)).getSize()
    width = width_cm * cm
    return Image(str(path), width=width, height=width * h / w)


def on_page(canvas, doc):
    canvas.saveState()
    canvas.setFont("DejaVu", 7.5)
    canvas.setFillColor(INK_2)
    canvas.drawString(1.8 * cm, 1.1 * cm, "Service Selection Optimizer — GLS × ILS e ablação da GLS")
    canvas.drawRightString(A4[0] - 1.8 * cm, 1.1 * cm, f"página {doc.page}")
    canvas.restoreState()


def comparison_section(data):
    labels = [
        ("ils1", "Java ILS#1 (First Improvement, SWAP, α=0,4)"),
        ("ils3", "Java ILS#3 (Best Improvement, MOVE, α=0,2)"),
        ("gls", "<b>C++ GLS promovida (I1 S1 L0 R1)</b>"),
    ]
    rows = []
    for key, label in labels:
        s10, s055 = summarize(data["10"][key]), summarize(data["0.55"][key])
        rows.append([label,
                     f"{s10['reached']}/94", fmt(s10["gap"]) + "%", fmt(s10["t"], 2) + " s",
                     f"{s055['reached']}/94", fmt(s055["gap"]) + "%", fmt(s055["t"], 3) + " s"])
    gls10, gls055 = data["10"]["gls"], data["0.55"]["gls"]
    runs10 = sum(v["optimal_runs"] for v in gls10.values())
    runs055 = sum(v["optimal_runs"] for v in gls055.values())
    ils3_055 = summarize(data["0.55"]["ils3"])
    over = sum(v["t"] > 0.6 for v in data["0.55"]["ils3"].values())
    story = [
        p("2. GLS × ILS do artigo", "h1"),
        p("Mesmas 94 instâncias, 3 repetições por instância e orçamento fixo por execução nos dois "
          "lados. O ILS é o código Java original dos autores (<i>ArticleResult.java</i>) nas "
          "configurações ILS#1 (a que o repositório dos autores executa) e ILS#3 (descrita no "
          "artigo). A GLS é a configuração promovida em C++."),
        table(["Método", "No ótimo<br/>10 s", "GAP<br/>10 s", "Tempo<br/>10 s",
               "No ótimo<br/>0,55 s", "GAP<br/>0,55 s", "Tempo<br/>0,55 s"],
              rows, [5.8 * cm, 1.8 * cm, 1.6 * cm, 1.7 * cm, 1.8 * cm, 1.6 * cm, 1.7 * cm],
              highlight_rows=(2,)),
        p("No ótimo = instâncias com pelo menos uma repetição no ótimo, de 94. Tempo médio: no Java, tempo até o melhor custo (ou o tempo total da repetição quando o "
          "ótimo não é atingido); na GLS, tempo até parar (ótimo atingido ou orçamento esgotado). "
          f"Execuções ótimas da GLS: {runs10}/282 a 10 s e {runs055}/282 a 0,55 s.", "note"),
        p("Leitura", "h2"),
        *bullets([
            "Com tempo de sobra (10 s), o ILS#3 chega perto (86/94), mas a GLS ainda vence em "
            "qualidade (GAP 0,04% contra 3,51%) e termina muito antes (0,26 s contra 2,17 s).",
            "Sob pressão de tempo (0,55 s), a diferença se abre: o ILS#3 cai para "
            f"{ils3_055['reached']}/94, enquanto a GLS mantém {summarize(gls055)['reached']}/94 "
            "com 0,12 s de média. A vantagem da GLS é de qualidade <b>e</b> de eficiência.",
            "O ILS#1, configuração efetivamente executada pelos autores, não atinge o ótimo em "
            "nenhuma instância em nenhum dos dois orçamentos.",
        ]),
        p("Ressalvas de tempo", "h2"),
        *bullets([
            f"A 0,55 s, o ILS#3 excede o orçamento em {over} das 94 instâncias (média de "
            f"{fmt(ils3_055['t'])} s, máximo de {fmt(ils3_055['max_t'])} s): o relógio é checado "
            "entre iterações, e cada busca local com Best Improvement vai até o ótimo local sem "
            "consultá-lo. O ILS#3 recebeu, na prática, cerca de 1,7× o tempo da GLS; o viés é a "
            "favor do ILS e não altera a conclusão.",
            "A 10 s, os dois ILS passam levemente do orçamento em algumas instâncias (até 11,6 s), "
            "pelo mesmo motivo.",
            "O ILS#1 a 10 s foi executado antes da inclusão da checagem de tempo dentro do laço de "
            "iterações do <i>ILS.java</i>; as demais execuções Java já usam essa checagem. A 10 s o "
            "efeito é desprezível.",
        ]),
    ]
    return story


def ablation_section():
    ablation.configure("0.55")
    s = ablation.stats(ablation.load())
    main_effects, interactions = ablation.factorial_effects(s)
    promoted = s[ablation.PROMOTED]
    name = ablation.config_name

    path_rows = []
    for key, label in ablation.PATH:
        v = s[key]
        path_rows.append([label.replace("\n", " "), name(key, v), f"{v['optimal_instances']}/94",
                          fmt(v["mean_gap"]) + "%", fmt(v["mean_time"], 3) + " s"])

    loo_rows = []
    for key, label in ablation.LEAVE_ONE_OUT:
        v = s[key]
        worse, better, equal = ablation.paired_counts(s, key)
        loo_rows.append([label, name(key, v), f"{v['optimal_instances']}/94",
                         f"{v['optimal_instances'] - promoted['optimal_instances']:+d}",
                         fmt(v["mean_gap"]) + "%",
                         ("+" if v["mean_gap"] >= promoted["mean_gap"] else "")
                         + fmt(v["mean_gap"] - promoted["mean_gap"]) + " pp",
                         f"{worse} / {better} / {equal}"])

    sr = [[s[k] for k in row] for row in ablation.SR_CELLS]

    def sr_cell(v):
        return f"{v['optimal_instances']}/94 · GAP {fmt(v['mean_gap'])}%"

    ranking = sorted(s.items(), key=lambda kv: (kv[1]["mean_gap"], kv[1]["mean_time"]))
    rank_rows, highlight = [], []
    for pos, (key, v) in enumerate(ranking, 1):
        if key == ablation.PROMOTED:
            highlight.append(pos - 1)
        rank_rows.append([str(pos), name(key, v), f"{v['optimal_instances']}/94",
                          f"{v['optimal_runs']}/282", fmt(v["mean_cost"]), fmt(v["mean_gap"]) + "%",
                          fmt(v["mean_time"], 3) + " s"])

    story = [
        p("3. Ablação das melhorias da GLS (0,55 s)", "h1"),
        p("Fatorial completo 2<super>4</super> sobre as quatro melhorias, todas sobre GFLS, mais a "
          "GLS pura (sem GFLS) como baseline: 17 configurações × 94 instâncias × 3 repetições, "
          "0,55 s fixos por execução. Usamos 0,55 s porque a 10 s as configurações boas encostam no "
          "teto de 93/94 instâncias e a contribuição de cada melhoria fica escondida."),
        table(["Sigla", "Melhoria"], [
            ["I", "Avaliação incremental da viabilidade"],
            ["S", "Substituição composta de serviço (vizinhança)"],
            ["L", "Lambda local (calibração do λ após a descida inicial)"],
            ["R", "Utilidade por arrependimento na escolha das penalidades"],
        ], [1.6 * cm, 14.4 * cm], align_left_cols=(1,)),
        p("3.1 Remoção de cada melhoria a partir da promovida", "h2"),
        p("É a leitura mais direta da importância de cada melhoria: quanto se perde ao tirar só "
          "ela. A última coluna compara instância a instância com a promovida."),
        table(["Variação", "Config.", "No ótimo", "Δ", "GAP médio", "Δ GAP",
               "Instâncias piores / melhores / iguais"],
              [["<b>Promovida</b>", "I1 S1 L0 R1", f"{promoted['optimal_instances']}/94", "—",
                fmt(promoted["mean_gap"]) + "%", "—", "—"]] + loo_rows,
              [4.0 * cm, 2.6 * cm, 1.6 * cm, 1.1 * cm, 1.6 * cm, 1.9 * cm, 3.2 * cm],
              highlight_rows=(0,)),
        p("3.2 Substituição de serviço e arrependimento só funcionam juntos", "h2"),
        p("Com a avaliação incremental ligada e o lambda local desligado:"),
        table(["", "Sem arrependimento (R0)", "Com arrependimento (R1)"],
              [["<b>Sem substituição (S0)</b>", sr_cell(sr[0][0]), sr_cell(sr[0][1])],
               ["<b>Com substituição (S1)</b>", sr_cell(sr[1][0]), "<b>" + sr_cell(sr[1][1]) + "</b>"]],
              [4.4 * cm, 5.8 * cm, 5.8 * cm], highlight_rows=(1,)),
        Spacer(1, 6),
        p("Isoladas, as duas não ajudam (a substituição) ou pioram (o arrependimento); juntas formam "
          "a melhor configuração. Por isso a construção acumulada abaixo cai ao adicionar a "
          "substituição e o efeito principal do arrependimento sai negativo no fatorial. As duas "
          "devem ser apresentadas como um par.", "body"),
        p("3.3 Construção acumulada até a promovida", "h2"),
        table(["Etapa", "Config.", "No ótimo", "GAP médio", "Tempo médio"], path_rows,
              [4.6 * cm, 4.2 * cm, 2.4 * cm, 2.4 * cm, 2.4 * cm], highlight_rows=(4,)),
        Spacer(1, 6),
        figure(CHARTS / "construcao_acumulada.png", 13.5),
        p("3.4 Efeitos principais e interações no fatorial", "h2"),
        p("Efeito principal = GAP médio das 8 configurações com a melhoria desligada menos o das 8 "
          "com ela ligada (positivo = a melhoria reduz o GAP em média). Sinergia positiva = ligar "
          "as duas juntas reduz o GAP mais do que a soma dos efeitos isolados."),
        table(["Melhoria", "GAP desligada", "GAP ligada", "Efeito (pp)"],
              [[n, fmt(off) + "%", fmt(on) + "%", ("+" if off >= on else "") + fmt(off - on)]
               for n, off, on in main_effects],
              [7 * cm, 3 * cm, 3 * cm, 3 * cm]),
        Spacer(1, 6),
        table(["Par", "Sinergia (pp)"],
              [[n, ("+" if v >= 0 else "") + fmt(v)] for n, v in interactions],
              [11 * cm, 5 * cm]),
        Spacer(1, 6),
        figure(CHARTS / "efeitos_principais.png", 15),
        p("3.5 Ranking das 17 configurações", "h2"),
        table(["#", "Configuração", "No ótimo", "Execuções ótimas", "Custo médio",
               "GAP médio", "Tempo médio"], rank_rows,
              [0.9 * cm, 4.3 * cm, 2.4 * cm, 2.4 * cm, 2 * cm, 2 * cm, 2 * cm],
              highlight_rows=highlight, align_left_cols=(1,),
              gap_cols={5: [v["mean_gap"] for _, v in ranking]}),
        p("Fundo azul mais escuro = GAP maior. Ranking por GAP médio; ele pode divergir da contagem "
          "de instâncias no ótimo porque o GAP médio é dominado por poucas instâncias difíceis "
          "(ex.: I0 S0 L1 R0 tem 84/94 instâncias no ótimo, mas GAP de 2,81%).", "note"),
    ]
    findings = [
        "<b>A promovida (I1 S1 L0 R1) é a melhor das 17</b> também na escala completa: "
        f"{promoted['optimal_instances']}/94 instâncias, GAP {fmt(promoted['mean_gap'])}%, "
        f"{fmt(promoted['mean_time'], 3)} s em média.",
        "<b>Substituição de serviço é a melhoria mais importante</b> (removê-la custa 76 instâncias), "
        "seguida do <b>arrependimento</b> (53), mas as duas dependem uma da outra.",
        "<b>Avaliação incremental</b> é decisiva sem as demais (15 → 66 instâncias) e moderada na "
        "promovida (7 instâncias).",
        "<b>GFLS sobre a GLS pura</b> praticamente não muda o resultado (14 → 15 instâncias).",
        "<b>Lambda local</b> ajuda nas configurações sem arrependimento (I1 S0 L1 R0 faz 85/94 e é a "
        "mais rápida, 0,098 s), mas não acrescenta nada à promovida (−3 instâncias). A decisão de "
        "mantê-lo desligado se confirma. Isso difere da ablação anterior com 5 instâncias, em que "
        "ele piorava o resultado em geral.",
    ]
    story += [p("3.6 Conclusões da ablação", "h2"), *bullets(findings)]
    return story


def per_instance_tables(data):
    story = []
    for letter, (budget, label) in zip("AB", (("0.55", "0,55 s"), ("10", "10 s"))):
        ils1, ils3, gls = data[budget]["ils1"], data[budget]["ils3"], data[budget]["gls"]
        ids = sorted(set(ils1) & set(ils3) & set(gls))
        rows, gaps = [], {3: [], 5: [], 7: []}
        for i in ids:
            g1, g3, gg = (gap(x[i]["mean"], x[i]["opt"]) for x in (ils1, ils3, gls))
            gaps[3].append(g1)
            gaps[5].append(g3)
            gaps[7].append(gg)
            rows.append([str(i), str(ils1[i]["opt"]), fmt(ils1[i]["mean"], 1), fmt(g1, 1) + "%",
                         fmt(ils3[i]["mean"], 1), fmt(g3, 1) + "%", fmt(gls[i]["mean"], 1),
                         fmt(gg, 1) + "%", fmt(gls[i]["t"], 3)])
        story += [
            PageBreak(),
            p(f"Apêndice {letter} — Resultados por instância, {label}", "h1"),
            p("Custo médio e GAP médio das 3 repetições. Cor do GAP, mesma escala nas três "
              "colunas: branco = 0% (ótimo); azul cada vez mais escuro para até 1%, até 5%, até 20% "
              "e acima de 20%.", "note"),
            table(["Inst.", "Ótimo", "ILS#1 médio", "ILS#1 GAP", "ILS#3 médio", "ILS#3 GAP",
                   "GLS médio", "GLS GAP", "GLS t (s)"], rows,
                  [1.3 * cm, 1.5 * cm, 1.9 * cm, 1.9 * cm, 1.9 * cm, 1.9 * cm, 1.9 * cm, 1.8 * cm,
                   1.8 * cm],
                  align_left_cols=(), gap_cols=gaps, font_size=7.5, banded=True),
        ]
    return story


def main():
    data = load_runs()
    s10 = {k: summarize(v) for k, v in data["10"].items()}
    s055 = {k: summarize(v) for k, v in data["0.55"].items()}
    story = [
        p("GLS × ILS do artigo e ablação da GLS", "title"),
        p(f"Service Selection Optimizer · relatório de experimentos · "
          f"{date.today().strftime('%d/%m/%Y')}", "subtitle"),
        p("Resumo", "h1"),
        *bullets([
            f"<b>A GLS promovida supera o ILS dos autores nos dois orçamentos.</b> A 10 s: "
            f"{s10['gls']['reached']}/94 instâncias no ótimo, contra {s10['ils3']['reached']}/94 do "
            f"ILS#3 e {s10['ils1']['reached']}/94 do ILS#1. A 0,55 s: {s055['gls']['reached']}/94, "
            f"contra {s055['ils3']['reached']}/94 e {s055['ils1']['reached']}/94.",
            "<b>A diferença cresce com pouco tempo:</b> o ILS#3 depende de tempo (86 → 7 instâncias "
            "de 10 s para 0,55 s), a GLS quase não perde (93 → 89).",
            "<b>Na ablação, substituição de serviço e arrependimento são as melhorias decisivas e "
            "só funcionam juntas;</b> a avaliação incremental contribui de forma moderada; GFLS e "
            "lambda local não acrescentam à configuração promovida.",
        ]),
        p("1. Protocolo experimental", "h1"),
        *bullets([
            "94 instâncias com ótimo conhecido (logs do método exato), 3 repetições por instância.",
            "Orçamento fixo por execução: 10 s (qualidade com tempo de sobra) e 0,55 s (eficiência "
            "sob pressão de tempo). A execução termina antes se atinge o ótimo conhecido.",
            "O protocolo original do artigo (orçamento = tempo do exato / 10 ou / 20) foi "
            "abandonado: dava menos de 0,2 s a 82 das 94 instâncias (mediana 0,06 s), tempo "
            "insuficiente para qualquer dos métodos convergir.",
            "Métricas: instâncias com pelo menos uma repetição no ótimo, GAP médio "
            "((custo − ótimo) / ótimo) e tempo médio.",
            "As 3 repetições partem da mesma solução inicial e saem quase idênticas; os números "
            "refletem o comportamento médio, não uma estimativa de variância.",
        ]),
        *comparison_section(data),
        *ablation_section(),
        p("4. Reprodutibilidade", "h1"),
        table(["Item", "Comando ou arquivo"], [
            ["GLS × ILS 10 s", "experiments/00_baseline_java/results/java_results_raw.txt, experiments/00_baseline_java/results/java_ils3_10s_raw.txt, "
             "experiments/07_protocolo_artigo/results/gls_promoted_article_protocol.csv"],
            ["GLS × ILS 0,55 s", "experiments/00_baseline_java/results/java_ils1_0.55s_raw.txt, "
             "experiments/00_baseline_java/results/java_ils3_0.55s_raw.txt, experiments/08_ablacao_gls/results/ablation_0.55s/i1_s1_l0_r1.csv"],
            ["Java (repositório dos autores)", "java -Dsso.ils=1|3 -Dsso.time=0.55|10 -cp \"bin:lib/*\" "
             "main.ArticleResult"],
            ["Ablação", "SSO_ABLATION_TIME=0.55 bash experiments/08_ablacao_gls/scripts/run_gls_ablation.sh"],
            ["Gráficos da ablação", "python3 experiments/08_ablacao_gls/scripts/plot_gls_ablation.py 0.55"],
            ["Tabelas por instância (PNG)", "python3 experiments/07_protocolo_artigo/scripts/plot_ils3_comparison.py [--budget 0.55 ...]"],
            ["Este relatório", "python3 experiments/08_ablacao_gls/scripts/build_report_pdf.py"],
        ], [4.2 * cm, 11.8 * cm], align_left_cols=(0, 1)),
        *per_instance_tables(data),
    ]
    doc = SimpleDocTemplate(str(OUTPUT), pagesize=A4, leftMargin=1.8 * cm, rightMargin=1.8 * cm,
                            topMargin=1.6 * cm, bottomMargin=1.8 * cm,
                            title="GLS × ILS do artigo e ablação da GLS",
                            author="Ramon Barros")
    doc.build(story, onFirstPage=on_page, onLaterPages=on_page)
    print(OUTPUT)


if __name__ == "__main__":
    main()
