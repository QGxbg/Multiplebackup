const {
  Document, Packer, Paragraph, TextRun, Table, TableRow, TableCell,
  BorderStyle, WidthType
} = require('docx');
const fs = require('fs');

const none  = { style: BorderStyle.NONE,   size: 0,  color: "FFFFFF" };
const thick = { style: BorderStyle.SINGLE, size: 12, color: "000000" };
const thin  = { style: BorderStyle.SINGLE, size: 4,  color: "000000" };
const hdrB  = { top: thick, bottom: thin,  left: none, right: none };
const lastB = { top: none,  bottom: thick, left: none, right: none };
const bodyB = { top: none,  bottom: none,  left: none, right: none };
const m = { top: 60, bottom: 60, left: 120, right: 120 };
const C = [900, 8460];

function row(ln, text, brd, bold, mono, italic) {
  return new TableRow({ children: [
    new TableCell({ borders: brd, margins: m, width: { size: C[0], type: WidthType.DXA },
      children: [new Paragraph({ spacing:{before:0,after:0}, children:[
        new TextRun({ text: ln, font:"Times New Roman", size:18 })]})]
    }),
    new TableCell({ borders: brd, margins: m, width: { size: C[1], type: WidthType.DXA },
      children: [new Paragraph({ spacing:{before:0,after:0}, children:[
        new TextRun({ text, font: mono?"Courier New":"Times New Roman",
          size:18, bold:!!bold, italics:!!italic })]})]
    })
  ]});
}

// [行号, 内容, 边框类型(h/b/l), 粗体, 等宽, 斜体]
const L = [
  ["","Algorithm 1  GenRepairBatchesForMultipleFailure(fail_node_ids, method)","h",1,0,0],
  ["","Input:  fail_node_ids, method          Output:  _batch_list","b",0,0,0],
  ["","","b",0,0,0],
  ["","// Phase 1: Bottleneck Estimation & Heavy-First Sort","b",0,0,1],
  ["1","filterFailedStripes(fail_node_ids)  ->  stripes_to_repair","b",0,1,0],
  ["2","if stripes_to_repair = empty  then  return","b",1,0,0],
  ["3","for each stripe s in stripes_to_repair  do","b",1,0,0],
  ["4","    dummy_load[s] <- RunDummyColoring(s, strategy=0)","b",0,1,0],
  ["5","    bottleneck[s] <- max_node { max(in_load, out_load) }","b",0,1,0],
  ["6","end for;   Sort stripes_to_repair descending by bottleneck[s]","b",1,0,0],
  ["","","b",0,0,0],
  ["","// Phase 2: Greedy 2-D Bin-Packing","b",0,0,1],
  ["7","num_batches <- ceil(|stripes_to_repair| / batch_size)","b",0,1,0],
  ["8","Initialize balanced_batches[0..num_batches-1] <- empty","b",1,0,0],
  ["9","for each stripe s in sorted stripes  do","b",1,0,0],
  ["10","    for each valid batch b  do","b",1,0,0],
  ["11","        sim_in/out <- batch_load[b] + dummy_load[s]  (per node)","b",0,1,0],
  ["12","        potential_bn[b] <- max_node { max(sim_in, sim_out) }","b",0,1,0],
  ["13","    end for","b",1,0,0],
  ["14","    Assign s -> batch with min potential_bn;   Update batch_load","b",1,0,0],
  ["15","end for","b",1,0,0],
  ["","","b",0,0,0],
  ["","// Phase 3: Simulated Annealing Optimization","b",0,0,1],
  ["16","if num_batches > 1  then","b",1,0,0],
  ["17","    T <- 1000;  alpha <- 0.95;  T_min <- 0.1","b",0,1,0],
  ["18","    current_sol <- balanced_batches;  best_energy <- Makespan(current_sol)","b",0,1,0],
  ["19","    while T > T_min  do","b",1,0,0],
  ["20","        Randomly swap s1 in b1 with s2 in b2  ->  neighbor_sol","b",0,1,0],
  ["21","        delta_E <- Makespan(neighbor_sol) - Makespan(current_sol)","b",0,1,0],
  ["22","        if delta_E < 0  or  rand(0,1) < exp(-delta_E/T)  then  Accept neighbor_sol","b",1,0,0],
  ["23","        Update best_sol if improved;   T <- T * alpha","b",1,0,0],
  ["24","    end while","b",1,0,0],
  ["25","    balanced_batches <- best_sol","b",0,1,0],
  ["26","end if","b",1,0,0],
  ["","","b",0,0,0],
  ["","// Phase 4: Adaptive Routing & Batch Assembly","b",0,0,1],
  ["27","for each batch b in balanced_batches  do","b",1,0,0],
  ["28","    for each stripe s in b  do","b",1,0,0],
  ["29","        Regenerate ECDAG for s","b",0,0,0],
  ["30","        if method = 1  then","b",1,0,0],
  ["31","            Simulate Hyper & Multi strategies  ->  time_h, time_m","b",0,1,0],
  ["32","            Apply strategy with lower batch bottleneck time","b",1,0,0],
  ["33","        else  ApplyColoring(s, method)","b",1,0,0],
  ["34","    end for","b",1,0,0],
  ["35","    curbatch <- new RepairBatch(b);  Evaluate & Enqueue curbatch","b",0,1,0],
  ["36","end for","b",1,0,0],
  ["","return  _batch_list","l",1,0,0],
];

const rows = L.map(([ln,text,bt,bold,mono,italic]) =>
  row(ln, text, bt==="h"?hdrB:bt==="l"?lastB:bodyB, bold, mono, italic));

const doc = new Document({
  sections: [{
    properties: { page: { size: { width:12240, height:15840 },
      margin: { top:1440, right:1440, bottom:1440, left:1440 } } },
    children: [
      new Paragraph({ spacing:{before:0,after:160}, children:[
        new TextRun({ text:"Table 1  Pseudocode of Algorithm GenRepairBatchesForMultipleFailure",
          font:"Times New Roman", size:20, bold:true })]}),
      new Table({ width:{size:9360,type:WidthType.DXA}, columnWidths:C, rows })
    ]
  }]
});

Packer.toBuffer(doc).then(buf => {
  fs.writeFileSync("pseudocode.docx", buf);
  console.log("生成成功: pseudocode.docx");
});