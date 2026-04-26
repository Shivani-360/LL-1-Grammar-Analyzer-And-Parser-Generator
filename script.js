/* ═══════════════════════════════════════════════════════════════
   LL(1) PARSER — COMPLETE SCRIPT
   Wizard flow: Input → Diagnose → Fix → Explore
   ═══════════════════════════════════════════════════════════════ */

// ── State ─────────────────────────────────────────────────────
let state = {
  grammarLines:  [],
  startSymbol:   '',
  diagData:      null,   // result from /diagnose (or /parse)
  fixData:       null,   // result from /auto-fix
  activeGrammar: [],     // lines being explored (fixed or original)
  activeStart:   '',
  firstSets:     null,
  followSets:    null,
  parsingTable:  null,
  isLL1:         false,
  conflicts:     [],
};

// ── Helpers ───────────────────────────────────────────────────
function esc(s) {
  return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}
function ins(sym) {
  let t = document.getElementById('grammarInput');
  let s = t.selectionStart, e = t.selectionEnd;
  t.value = t.value.substring(0,s) + sym + t.value.substring(e);
  t.selectionStart = t.selectionEnd = s + sym.length;
  t.focus();
}
function setStep(n) {
  for (let i=1; i<=4; i++) {
    let el = document.getElementById('step-dot-' + i);
    el.classList.remove('active','done');
    if (i < n)  el.classList.add('done');
    if (i === n) el.classList.add('active');
  }
}
function showPhase(n) {
  document.querySelectorAll('.phase').forEach(p => p.classList.add('hidden'));
  document.getElementById('phase' + n).classList.remove('hidden');
  setStep(n);
  window.scrollTo({ top: 0, behavior: 'smooth' });
}
function goBack(n) { showPhase(n); }
function startOver() {
  state = { grammarLines:[], startSymbol:'', diagData:null, fixData:null,
            activeGrammar:[], activeStart:'', firstSets:null, followSets:null,
            parsingTable:null, isLL1:false, conflicts:[] };
  showPhase(1);
}

// ── Load Examples ─────────────────────────────────────────────
const EXAMPLES = {
  arithmetic: { g:"E->TE'\nE'->+TE'|#\nT->FT'\nT'->*FT'|#\nF->(E)|id", s:"E" },
  parens:     { g:"S->(S)S|#",                                          s:"S" },
  ifelse:     { g:"S->iEtSS'\nS'->eS|#\nE->b",                         s:"S" },
  decl:       { g:"D->TL\nT->int|float\nL->idR\nR->,idR|#",            s:"D" },
  relop:      { g:"E->TA'\nA'->+TA'|#\nT->FB'\nB'->*FB'|#\nF->id|num|(E)", s:"E" },
  leftrecur:  { g:"E->E+T|T\nT->T*F|F\nF->id|(E)",                     s:"E" },
  leftfactor: { g:"S->iEtS|iEtSeS|a\nE->b",                            s:"S" },
  both:       { g:"A->Aa|Ab|c\nB->Bd|e|Be",                            s:"A" },
};
function loadExample() {
  let sel = document.getElementById('exampleSelect').value;
  if (EXAMPLES[sel]) {
    document.getElementById('grammarInput').value = EXAMPLES[sel].g;
    document.getElementById('startSymbol').value  = EXAMPLES[sel].s;
  }
}

// ── PHASE 1 → 2: Diagnose ────────────────────────────────────
async function runDiagnose() {
  let gText = document.getElementById('grammarInput').value.trim();
  let start = document.getElementById('startSymbol').value.trim();
  let lines = gText.split('\n').map(l => l.trim()).filter(l => l !== '');

  if (!lines.length) { alert('Please enter grammar rules.'); return; }
  if (!start)        { alert('Please enter a start symbol.'); return; }

  state.grammarLines = lines;
  state.startSymbol  = start;

  let btn = document.querySelector('#phase1 .btn-primary');
  btn.textContent = 'Analysing…';
  btn.disabled = true;

  try {
    let res  = await fetch('http://localhost:8080/auto-fix', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ grammar: lines, start_symbol: start })
    });
    let data = await res.json();
    state.diagData = data;
    renderDiagnosis(data);
    showPhase(2);
  } catch(e) {
    alert('Cannot connect to server. Make sure server.exe is running on port 8080.');
  } finally {
    btn.textContent = 'Analyse Grammar →';
    btn.disabled = false;
  }
}

function renderDiagnosis(data) {
  let errorBox  = document.getElementById('errorBox');
  let diagGrid  = document.getElementById('diagGrid');
  let allClear  = document.getElementById('allClearBox');
  let fixBtn    = document.getElementById('fixBtn');
  let skipBtn   = document.getElementById('skipFixBtn');

  errorBox.innerHTML = '';
  errorBox.classList.add('hidden');
  diagGrid.innerHTML = '';
  allClear.classList.add('hidden');
  fixBtn.style.display  = 'none';
  skipBtn.style.display = 'none';

  // Structural errors
  if (!data.success && data.errors && data.errors.length) {
    let html = `<div class="error-box"><h4>❌ Grammar Errors — Fix These First</h4>`;
    data.errors.forEach(e => html += `<div class="error-item">• ${esc(e)}</div>`);
    html += `</div>`;
    errorBox.innerHTML = html;
    errorBox.classList.remove('hidden');
    return;
  }

  // Build 3 diagnosis cards
  let checks = [
    {
      key:   'lr',
      icon:  '🔄',
      title: 'Left Recursion',
      desc:  'A non-terminal derives itself as the leftmost symbol (e.g. A → Aα). This causes LL(1) parsers to loop forever.',
      found: data.had_left_recursion,
      details: (data.fix_steps||[]).filter(s => s.includes('Left Recursion in')).map(s => s.replace('Left Recursion in ',''))
    },
    {
      key:   'lf',
      icon:  '🔀',
      title: 'Left Factoring',
      desc:  'Two alternatives share a common prefix (A → αβ | αγ). The parser cannot decide which rule to pick with one lookahead.',
      found: data.had_left_factoring,
      details: (data.fix_steps||[]).filter(s => s.includes('Left Factoring in')).map(s => s.replace('Left Factoring in ',''))
    },
    {
      key:   'amb',
      icon:  '🌀',
      title: 'Ambiguity',
      desc:  'A string can be parsed in more than one way (multiple parse trees). Detected by conflicts in the parsing table.',
      found: data.had_ambiguity,
      details: data.had_ambiguity ? (data.conflicts||[]).slice(0,3) : []
    },
  ];

  checks.forEach(c => {
    let cls = c.found ? 'found' : 'clean';
    let statusTxt = c.found ? '⚠ Detected' : '✓ None Found';
    let detailHtml = '';
    if (c.found && c.details.length) {
      detailHtml = `<div class="diag-detail">` +
        c.details.slice(0,3).map(d => `• ${esc(d)}`).join('<br>') +
        (c.details.length > 3 ? `<br>… and ${c.details.length - 3} more` : '') +
        `</div>`;
    }
    diagGrid.innerHTML += `
      <div class="diag-card ${cls}">
        <div class="diag-icon">${c.icon}</div>
        <div class="diag-title">${c.title}</div>
        <div class="diag-status">${statusTxt}</div>
        <div class="diag-desc">${c.desc}</div>
        ${detailHtml}
      </div>`;
  });

  let anyFound = data.had_left_recursion || data.had_left_factoring || data.had_ambiguity;

  if (!anyFound) {
    allClear.classList.remove('hidden');
    // Grammar is clean — preload results and skip to explore
    if (data.first_sets) {
      state.firstSets    = data.first_sets;
      state.followSets   = data.follow_sets;
      state.parsingTable = data.parsing_table;
      state.isLL1        = data.isLL1;
      state.conflicts    = data.conflicts || [];
      state.activeGrammar = state.grammarLines;
      state.activeStart   = state.startSymbol;
      state.fixData       = data;
    }
    skipBtn.style.display = '';
  } else {
    if (!data.had_ambiguity || data.had_left_recursion || data.had_left_factoring) {
      fixBtn.style.display = '';
    } else {
      // Only ambiguity (can't auto-fix) — still let them explore
      skipBtn.style.display = '';
    }
  }
}

// ── PHASE 2 → 3: Run Fix ─────────────────────────────────────
async function runFix() {
  // data is already in state.diagData from the auto-fix call
  let data = state.diagData;
  if (!data) return;
  state.fixData = data;

  // Set active grammar to the fixed version
  if (data.was_fixed && data.fixed_grammar_lines) {
    state.activeGrammar = data.fixed_grammar_lines;
    state.activeStart   = data.fixed_start_symbol;
  } else {
    state.activeGrammar = state.grammarLines;
    state.activeStart   = state.startSymbol;
  }
  state.firstSets    = data.first_sets;
  state.followSets   = data.follow_sets;
  state.parsingTable = data.parsing_table;
  state.isLL1        = data.isLL1;
  state.conflicts    = data.conflicts || [];

  renderFixExplanation(data);
  showPhase(3);
}

function renderFixExplanation(data) {
  let box = document.getElementById('fixContent');
  box.innerHTML = '';

  let steps = data.fix_steps || [];

  // Group steps by section
  let sections = [];
  let cur = null;
  steps.forEach(s => {
    if (s.startsWith('===')) {
      if (cur) sections.push(cur);
      cur = { title: s.replace(/===/g,'').trim(), lines: [] };
    } else if (cur) {
      cur.lines.push(s);
    }
  });
  if (cur) sections.push(cur);

  sections.forEach(sec => {
    let icon = sec.title.includes('LEFT RECURSION') ? '🔄' :
               sec.title.includes('LEFT FACTORING') ? '🔀' : '🌀';

    let linesHtml = sec.lines.map(l => {
      if (l.startsWith('  Result:') || l.startsWith('  '))
        return `<div class="ts-detail">${esc(l)}</div>`;
      return `<div class="ts-info">${esc(l)}</div>`;
    }).join('');

    box.innerHTML += `
      <div class="fix-section">
        <div class="fix-section-head">
          <span style="font-size:1.2rem">${icon}</span>
          <h3>${esc(sec.title)}</h3>
        </div>
        <div class="fix-section-body">
          <div class="fix-steps-terminal">${linesHtml}</div>
        </div>
      </div>`;
  });

  // Show fixed grammar
  if (data.was_fixed && data.fixed_grammar_lines) {
    let gramHtml = data.fixed_grammar_lines.map((line, i) => {
      let isNew = data.had_left_recursion || data.had_left_factoring;
      let cls = isNew && line.includes("'") ? 'g-new' : '';
      let parts = line.split('->');
      if (parts.length >= 2) {
        return `<div class="${cls}"><span class="g-nt">${esc(parts[0].trim())}</span>`+
               `<span class="g-arrow"> → </span>`+
               `<span class="g-prod">${esc(parts.slice(1).join('->').trim())}</span></div>`;
      }
      return `<div>${esc(line)}</div>`;
    }).join('');

    box.innerHTML += `
      <div class="fix-section">
        <div class="fix-section-head">
          <span style="font-size:1.2rem">✅</span>
          <h3>FIXED GRAMMAR</h3>
        </div>
        <div class="fix-section-body">
          <p style="font-size:0.82rem;color:var(--text-dim);margin-bottom:12px;">
            New non-terminals (highlighted) were introduced to eliminate recursion/factoring.
          </p>
          <div class="fixed-grammar-display">${gramHtml}</div>
          <button class="use-grammar-btn" onclick="loadFixedIntoInput()">
            📋 Load Fixed Grammar into Input Box
          </button>
        </div>
      </div>`;
  }

  // Ambiguity note
  let ambMsg = data.ambiguity_message || '';
  if (ambMsg) {
    let isOk = !data.had_ambiguity;
    box.innerHTML += `
      <div class="fix-section">
        <div class="fix-section-head">
          <span style="font-size:1.2rem">${isOk ? '✅' : '⚠️'}</span>
          <h3>AMBIGUITY CHECK</h3>
        </div>
        <div class="fix-section-body">
          <p style="font-size:0.88rem;color:${isOk ? 'var(--green)' : 'var(--yellow)'}">
            ${esc(ambMsg)}
          </p>
        </div>
      </div>`;
  }
}

function loadFixedIntoInput() {
  if (state.fixData && state.fixData.fixed_grammar_lines) {
    document.getElementById('grammarInput').value = state.fixData.fixed_grammar_lines.join('\n');
    document.getElementById('startSymbol').value  = state.fixData.fixed_start_symbol;
    alert('Fixed grammar loaded. You can now re-run Analyse Grammar if needed, or continue to Explore.');
  }
}

// ── PHASE 4: Explore ─────────────────────────────────────────
function goToExplore() {
  if (!state.activeGrammar.length) {
    // Fallback: use original
    state.activeGrammar = state.grammarLines;
    state.activeStart   = state.startSymbol;
  }

  renderActiveGrammar();
  renderFirstSets();
  renderFollowSets();
  renderParsingTable();
  showPhase(4);
}

function renderActiveGrammar() {
  let box = document.getElementById('activeGrammarBox');
  let linesHtml = state.activeGrammar.map(l => {
    let p = l.split('->');
    if (p.length >= 2) {
      return `<span class="g-nt">${esc(p[0].trim())}</span>`+
             `<span class="g-arrow"> → </span>`+
             `<span class="g-prod">${esc(p.slice(1).join('->').trim())}</span>`;
    }
    return esc(l);
  }).join('<br>');

  box.innerHTML = `
    <div>
      <div class="agb-label">Active Grammar</div>
      <div class="agb-content">${linesHtml}</div>
    </div>
    <div class="agb-divider"></div>
    <div>
      <div class="agb-label">Start Symbol</div>
      <div class="agb-content agb-start">${esc(state.activeStart)}</div>
    </div>`;
}

function renderFirstSets() {
  let div = document.getElementById('firstResult');
  if (!state.firstSets) { div.innerHTML = '<p class="placeholder-msg">Not available.</p>'; return; }
  let html = '<div class="sets-grid">';
  Object.keys(state.firstSets).sort().forEach(nt => {
    let tokens = state.firstSets[nt];
    let tokHtml = tokens.map(t =>
      `<span class="set-token ${t==='ε'?'eps':''}">${esc(t)}</span>`
    ).join('');
    html += `<div class="set-card">
      <div class="set-nt">FIRST( ${esc(nt)} )</div>
      <div class="set-val">{ ${tokHtml} }</div>
    </div>`;
  });
  html += '</div>';
  div.innerHTML = html;
}

function renderFollowSets() {
  let div = document.getElementById('followResult');
  if (!state.followSets) { div.innerHTML = '<p class="placeholder-msg">Not available.</p>'; return; }
  let html = '<div class="sets-grid">';
  Object.keys(state.followSets).sort().forEach(nt => {
    let tokens = state.followSets[nt];
    let tokHtml = tokens.map(t =>
      `<span class="set-token ${t==='$'?'eps':''}">${esc(t)}</span>`
    ).join('');
    html += `<div class="set-card">
      <div class="set-nt">FOLLOW( ${esc(nt)} )</div>
      <div class="set-val">{ ${tokHtml} }</div>
    </div>`;
  });
  html += '</div>';
  div.innerHTML = html;
}

function renderParsingTable() {
  let div      = document.getElementById('tableResult');
  let confDiv  = document.getElementById('conflictResult');

  if (!state.parsingTable) {
    div.innerHTML = '<p class="placeholder-msg">Not available.</p>';
    return;
  }

  let table = state.parsingTable;
  let terminals = [];
  Object.values(table).forEach(row => Object.keys(row).forEach(t => { if (!terminals.includes(t)) terminals.push(t); }));
  if (!terminals.includes('$')) terminals.push('$');
  terminals.sort();

  let conflictMap = {};
  (state.conflicts||[]).forEach(c => {
    let m = c.match(/Conflict at \[(.+?)\]\[(.+?)\]/);
    if (m) conflictMap[m[1]+'||'+m[2]] = true;
  });

  let html = '<div class="table-wrap"><table class="parse-table"><tr><th>NT \\ T</th>';
  terminals.forEach(t => html += `<th>${esc(t)}</th>`);
  html += '</tr>';

  Object.keys(table).sort().forEach(nt => {
    html += `<tr><td class="nt-col">${esc(nt)}</td>`;
    terminals.forEach(t => {
      let key = nt+'||'+t;
      let rule = table[nt] && table[nt][t];
      if (conflictMap[key]) {
        html += `<td class="conflict" title="${esc(state.conflicts.find(c=>c.includes('['+nt+']['+t+']'))||'')}">CONFLICT</td>`;
      } else if (rule) {
        html += `<td class="has-rule" title="${esc(rule)}">${esc(rule)}</td>`;
      } else {
        html += `<td class="empty">—</td>`;
      }
    });
    html += '</tr>';
  });
  html += '</table></div>';

  if (!state.isLL1 && state.conflicts.length) {
    html += `<div class="conflict-section">
      <h4>⚠ Conflicts — Grammar is NOT LL(1)</h4>
      ${state.conflicts.map(c => `<div class="conflict-item">• ${esc(c)}</div>`).join('')}
    </div>`;
  } else {
    html += `<div class="ll1-ok">✅ No conflicts — Grammar is LL(1)!</div>`;
  }

  div.innerHTML = html;
  confDiv.innerHTML = '';
  confDiv.classList.add('hidden');
}

// ── Accordion toggle ──────────────────────────────────────────
function toggleAcc(id) {
  let acc  = document.getElementById(id);
  let body = acc.querySelectorAll('.acc-body');
  let isOpen = acc.classList.contains('open');

  if (isOpen) {
    acc.classList.remove('open');
    body.forEach(b => b.classList.add('hidden'));
  } else {
    acc.classList.add('open');
    body.forEach(b => b.classList.remove('hidden'));
  }
}

// ── JS-Based LL(1) Parser (no server needed) ─────────────────
function jsTokenise(inputStr, terminals) {
  let tokens = [];
  let sorted = [...terminals].sort((a,b) => b.length - a.length);
  let pos = 0;
  while (pos < inputStr.length) {
    if (inputStr[pos] === ' ') { pos++; continue; }
    let matched = false;
    for (let t of sorted) {
      if (inputStr.substr(pos, t.length) === t) {
        tokens.push(t);
        pos += t.length;
        matched = true;
        break;
      }
    }
    if (!matched) { tokens.push(inputStr[pos]); pos++; }
  }
  tokens.push('$');
  return tokens;
}

function parseRhsSymbols(rhs, nonTerminals) {
  let symbols = [];
  let sortedNTs = [...nonTerminals].sort((a,b) => b.length - a.length);
  let pos = 0;
  while (pos < rhs.length) {
    if (rhs[pos] === ' ') { pos++; continue; }
    let foundNT = false;
    for (let nt of sortedNTs) {
      if (rhs.substr(pos, nt.length) === nt) {
        symbols.push(nt);
        pos += nt.length;
        foundNT = true;
        break;
      }
    }
    if (!foundNT) {
      // try multi-char lowercase terminal
      let j = pos;
      while (j < rhs.length && /[a-z0-9]/.test(rhs[j])) j++;
      if (j - pos > 1) { symbols.push(rhs.substring(pos, j)); pos = j; }
      else { symbols.push(rhs[pos]); pos++; }
    }
  }
  return symbols;
}

function buildGrammarFromLines(lines) {
  let nonTerminals = new Set();
  let terminals = new Set();
  let productions = {}; // NT -> [[sym,...], ...]
  let startSymbol = null;

  lines.forEach(line => {
    let sep = line.indexOf('->');
    if (sep < 0) return;
    let lhs = line.substring(0, sep).trim();
    let rhsPart = line.substring(sep+2).trim();
    nonTerminals.add(lhs);
    if (!startSymbol) startSymbol = lhs;
    if (!productions[lhs]) productions[lhs] = [];
    let alts = rhsPart.split('|');
    alts.forEach(alt => {
      alt = alt.trim();
      productions[lhs].push(alt === '#' || alt === 'ε' ? ['ε'] : [alt]);
    });
  });

  // Identify terminals
  Object.values(productions).flat().forEach(altArr => {
    let rhs = altArr[0];
    if (rhs === 'ε') return;
    let syms = parseRhsSymbols(rhs, nonTerminals);
    syms.forEach(s => { if (!nonTerminals.has(s)) terminals.add(s); });
  });

  return { nonTerminals, terminals, productions, startSymbol };
}

function jsLL1Parse(inputStr, grammar, parsingTable) {
  let { nonTerminals, terminals, startSymbol } = grammar;
  let tokens = jsTokenise(inputStr, terminals);
  let steps = [];
  let accepted = false;

  // Parse tree node structure
  let treeRoot = { label: startSymbol, children: [] };
  let stackNodes = [{ sym: '$', node: null }, { sym: startSymbol, node: treeRoot }];
  let ip = 0;

  let stackStr = () => [...stackNodes].reverse().map(s => s.sym).join(' ');
  let inputStr2 = () => tokens.slice(ip).join(' ');

  while (true) {
    let top = stackNodes[stackNodes.length - 1];
    let curToken = ip < tokens.length ? tokens[ip] : '$';

    let step = { stack: stackStr(), input: inputStr2(), action: '', isError: false };

    if (top.sym === '$' && curToken === '$') {
      step.action = "Accept — string is valid!";
      steps.push(step);
      accepted = true;
      break;
    }
    if (top.sym === '$') {
      step.action = "Error — stack empty but input remains: " + curToken;
      step.isError = true;
      steps.push(step);
      break;
    }
    if (!nonTerminals.has(top.sym)) {
      // terminal
      if (top.sym === curToken) {
        step.action = "Match terminal '" + top.sym + "', advance input";
        if (top.node) top.node.matched = true;
        stackNodes.pop();
        ip++;
      } else {
        step.action = "Error — expected '" + top.sym + "' but found '" + curToken + "'";
        step.isError = true;
        steps.push(step);
        break;
      }
      steps.push(step);
      continue;
    }

    // Non-terminal
    let row = parsingTable[top.sym];
    let rule = row && row[curToken];
    if (!rule) {
      step.action = "Error — no rule in table[" + top.sym + "][" + curToken + "]";
      step.isError = true;
      steps.push(step);
      break;
    }

    step.action = "Apply rule: " + rule;
    steps.push(step);
    stackNodes.pop();

    let arrowPos = rule.indexOf('->');
    let rhs = rule.substring(arrowPos + 2).trim();

    let children = [];
    if (rhs !== 'ε' && rhs !== '#') {
      let syms = parseRhsSymbols(rhs, nonTerminals);
      syms.forEach(s => {
        let child = { label: s, children: [] };
        children.push(child);
        if (top.node) top.node.children.push(child);
      });
      // push in reverse
      for (let i = children.length - 1; i >= 0; i--) {
        stackNodes.push({ sym: children[i].label, node: children[i] });
      }
    } else {
      let epsChild = { label: 'ε', children: [], isEps: true };
      if (top.node) top.node.children.push(epsChild);
    }
  }

  return { steps, accepted, treeRoot };
}

// ── Parse String ─────────────────────────────────────────────
let traceSteps = [], traceIdx = 0;
let lastParseTree = null;

function parseString() {
  let inputStr = document.getElementById('stringInput').value.trim();
  if (!inputStr) { alert('Please enter a string to parse.'); return; }
  if (!state.activeGrammar.length) { alert('No active grammar. Please complete the analysis first.'); return; }
  if (!state.isLL1) {
    document.getElementById('traceResult').innerHTML =
      '<p style="color:var(--red);font-family:var(--mono);font-size:0.85rem">Cannot parse — grammar is not LL(1) (has conflicts).</p>';
    return;
  }

  let grammar = buildGrammarFromLines(state.activeGrammar);
  grammar.startSymbol = state.activeStart;

  let result = jsLL1Parse(inputStr, grammar, state.parsingTable);
  lastParseTree = result.accepted ? result.treeRoot : null;

  renderTrace({ success: true, accepted: result.accepted, steps: result.steps });

  // Update parse tree section
  let treeResult = document.getElementById('treeResult');
  if (treeResult) {
    if (result.accepted && lastParseTree) {
      renderParseTree(lastParseTree, treeResult);
      // auto-open the tree accordion
      let treeAcc = document.getElementById('acc-tree');
      if (treeAcc && !treeAcc.classList.contains('open')) {
        treeAcc.classList.add('open');
        treeAcc.querySelectorAll('.acc-body').forEach(b => b.classList.remove('hidden'));
      }
    } else {
      treeResult.innerHTML = '<p style="color:var(--red);font-family:var(--mono);font-size:0.85rem;padding:12px 0">String was rejected — no parse tree generated.</p>';
    }
  }
}

function renderTrace(data) {
  let div = document.getElementById('traceResult');
  if (!data.success) {
    div.innerHTML = `<p style="color:var(--red);font-family:var(--mono);font-size:0.85rem">Error: ${esc(data.error)}</p>`;
    return;
  }

  traceSteps = data.steps;
  traceIdx   = 0;

  let bannerCls = data.accepted ? 'accepted' : 'rejected';
  let bannerTxt = data.accepted ? '✅  ACCEPTED — String is a valid sentence of this grammar!'
                                : '❌  REJECTED — String is NOT valid in this grammar.';

  div.innerHTML = `
    <div class="trace-result-banner ${bannerCls}">${bannerTxt}</div>
    <div class="trace-nav">
      <button onclick="traceNav(-1)">◀ Prev</button>
      <span class="counter" id="traceCounter">Step 1 of ${traceSteps.length}</span>
      <button onclick="traceNav(1)">Next ▶</button>
      <button class="show-all-btn" onclick="toggleAllSteps()">Show All</button>
    </div>
    <div id="traceStep"></div>
    <div id="traceAll" class="hidden"></div>`;

  renderStep();
  renderAllSteps();
}

function traceNav(d) {
  traceIdx = Math.max(0, Math.min(traceSteps.length-1, traceIdx+d));
  renderStep();
  document.getElementById('traceCounter').textContent =
    'Step ' + (traceIdx+1) + ' of ' + traceSteps.length;
}

function renderStep() {
  let s   = traceSteps[traceIdx];
  let cls = s.isError ? 'trace-row error-row' : (traceIdx === traceSteps.length-1 ? 'trace-row accept-row' : 'trace-row');
  document.getElementById('traceStep').innerHTML = `
    <table class="trace-table">
      <tr><th>#</th><th>Stack</th><th>Remaining Input</th><th>Action</th></tr>
      <tr class="${cls}">
        <td>${traceIdx+1}</td>
        <td><code>${esc(s.stack)}</code></td>
        <td><code>${esc(s.input)}</code></td>
        <td>${esc(s.action)}</td>
      </tr>
    </table>`;
}

function renderAllSteps() {
  let html = `<table class="trace-table">
    <tr><th>#</th><th>Stack</th><th>Remaining Input</th><th>Action</th></tr>`;
  traceSteps.forEach((s,i) => {
    let cls = s.isError ? 'trace-row error-row' :
      (i===traceSteps.length-1 && !s.isError ? 'trace-row accept-row' : 'trace-row');
    html += `<tr class="${cls}">
      <td>${i+1}</td>
      <td><code>${esc(s.stack)}</code></td>
      <td><code>${esc(s.input)}</code></td>
      <td>${esc(s.action)}</td>
    </tr>`;
  });
  html += '</table>';
  document.getElementById('traceAll').innerHTML = html;
}

function toggleAllSteps() {
  let all  = document.getElementById('traceAll');
  let step = document.getElementById('traceStep');
  let nav  = document.querySelector('.trace-nav');
  let btn  = document.querySelector('.show-all-btn');
  if (all.classList.contains('hidden')) {
    all.classList.remove('hidden');
    step.classList.add('hidden');
    nav.style.display = 'none';
    btn.style.display = 'inline-block';
    btn.textContent = 'Step View';
  } else {
    all.classList.add('hidden');
    step.classList.remove('hidden');
    nav.style.display = 'flex';
    btn.textContent = 'Show All';
  }
}

// ── Parse Tree Rendering ──────────────────────────────────────
function renderParseTree(root, container) {
  // Build an SVG parse tree using a simple layout algorithm
  const NODE_W = 54, NODE_H = 32, V_GAP = 52, H_GAP = 12;

  function measureTree(node) {
    if (!node.children || node.children.length === 0) {
      node._width = NODE_W + H_GAP;
      return;
    }
    node.children.forEach(measureTree);
    node._width = node.children.reduce((s, c) => s + c._width, 0);
    if (node._width < NODE_W + H_GAP) node._width = NODE_W + H_GAP;
  }

  function assignPos(node, x, depth) {
    node._depth = depth;
    if (!node.children || node.children.length === 0) {
      node._x = x + node._width / 2;
      return;
    }
    let cx = x;
    node.children.forEach(child => {
      assignPos(child, cx, depth + 1);
      cx += child._width;
    });
    node._x = (node.children[0]._x + node.children[node.children.length - 1]._x) / 2;
  }

  function maxDepth(node) {
    if (!node.children || node.children.length === 0) return node._depth;
    return Math.max(...node.children.map(maxDepth));
  }

  function collectEdges(node, edges) {
    if (!node.children) return;
    node.children.forEach(child => {
      edges.push({ x1: node._x, y1: node._depth, x2: child._x, y2: child._depth });
      collectEdges(child, edges);
    });
  }

  function collectNodes(node, nodes) {
    nodes.push(node);
    if (node.children) node.children.forEach(c => collectNodes(c, nodes));
  }

  measureTree(root);
  assignPos(root, 0, 0);

  let totalDepth = maxDepth(root);
  let svgW = Math.max(root._width, 300);
  let svgH = (totalDepth + 1) * (NODE_H + V_GAP) + 20;

  let edges = [], nodes = [];
  collectEdges(root, edges);
  collectNodes(root, nodes);

  const px = x => x;
  const py = d => d * (NODE_H + V_GAP) + NODE_H / 2 + 10;

  let edgeSvg = edges.map(e =>
    `<line x1="${px(e.x1)}" y1="${py(e.y1)}" x2="${px(e.x2)}" y2="${py(e.y2)}" stroke="var(--border)" stroke-width="1.5"/>`
  ).join('');

  let nodeSvg = nodes.map(n => {
    let isLeaf = !n.children || n.children.length === 0;
    let isEps  = n.isEps;
    let fill   = isEps ? 'var(--surface2)' : isLeaf ? 'var(--green)' : 'var(--accent)';
    let stroke = isLeaf && !isEps ? 'var(--green)' : 'var(--accent)';
    let textCol = isLeaf && !isEps ? '#000' : '#fff';
    let label  = n.label.length > 6 ? n.label.substring(0, 6) : n.label;
    return `
      <rect x="${px(n._x) - NODE_W/2}" y="${py(n._depth) - NODE_H/2}"
            width="${NODE_W}" height="${NODE_H}" rx="6"
            fill="${fill}" stroke="${stroke}" stroke-width="1.5"/>
      <text x="${px(n._x)}" y="${py(n._depth) + 5}" text-anchor="middle"
            font-family="var(--mono)" font-size="12" fill="${textCol}" font-weight="600">${esc(label)}</text>`;
  }).join('');

  let svg = `<svg viewBox="0 0 ${svgW} ${svgH}" width="${svgW}" height="${svgH}"
    xmlns="http://www.w3.org/2000/svg" style="max-width:100%;overflow-x:auto">
    ${edgeSvg}${nodeSvg}
  </svg>`;

  container.innerHTML = `
    <div style="overflow-x:auto;padding:8px 0">
      <div style="font-size:0.78rem;color:var(--text-dim);margin-bottom:10px">
        <span style="display:inline-block;width:14px;height:14px;background:var(--accent);border-radius:3px;vertical-align:middle;margin-right:4px"></span> Non-terminal &nbsp;
        <span style="display:inline-block;width:14px;height:14px;background:var(--green);border-radius:3px;vertical-align:middle;margin-right:4px"></span> Terminal &nbsp;
        <span style="display:inline-block;width:14px;height:14px;background:var(--surface2);border:1px solid var(--accent);border-radius:3px;vertical-align:middle;margin-right:4px"></span> ε
      </div>
      ${svg}
    </div>`;
}