"use strict";

const $ = (id) => document.getElementById(id);
const kinds = {
    npc : "NPC",
    monster : "몹",
    wildlife : "야생동물",
    player : "플레이어"
};
const roles = {
    resident : "주민",
    merchant : "상인",
    guard : "경비",
    healer : "치유사 / 성직자",
    scholar : "학자",
    bandit : "산적",
    pilgrim : "순례자",
    beast : "야수",
    test_beast : "야수",
    player : "플레이어",
    wildlife : "야생동물"
};
const labels = {
    hunger : "배고픔",
    thirst : "갈증",
    fatigue : "피로",
    food : "보유 식량",
    medicine : "보유 약품",
    coins : "보유 화폐",
    fear_memory : "공포 기억",
    action_held_seconds : "행동 유지 (초)",
    surrendered : "항복 (1=예)",
    respawn_remaining_seconds : "재생성 대기 (초)",
    attack_windup_seconds : "공격 준비 (초)",
    courage : "용기",
    empathy : "공감",
    aggression : "공격성",
    lawfulness : "준법성",
    greed : "탐욕",
    curiosity : "호기심",
    loyalty : "충성",
    level : "레벨",
    experience : "경험치",
    defeated : "처치 수"
};
let records = [], selectedWorld = "", selectedEntity = "", selectedEpoch = "", page = 0;
let receiverOnline = false, lastFetch = 0, townSignature = "";
const pageSize = 50;

function element(tag, text, className)
{
    const node = document.createElement(tag);
    if (text !== undefined)
        node.textContent = text;
    if (className)
        node.className = className;
    return node;
}

function key(snapshot)
{
    return JSON.stringify([ snapshot.game_id, snapshot.world_id, snapshot.instance_id ]);
}

function current()
{
    return records.find((r) => key(r.snapshot) === selectedWorld);
}

function format(value)
{
    return value == null               ? "미수집"
           : typeof value === "number" ? value.toLocaleString("ko-KR", {maximumFractionDigits : 2})
                                       : String(value);
}

function age(record)
{
    return record.age_seconds + Math.max(0, (performance.now() - lastFetch) / 1000);
}

function stale(record)
{
    return !receiverOnline || age(record) > 5;
}

function count(summary, kind)
{
    const c = summary.counts[kind];
    return `${c.alive} / ${c.total}`;
}

function render()
{
    if (!records.some((r) => key(r.snapshot) === selectedWorld))
    {
        selectedWorld = records.length
                            ? key(records.find((r) => !stale(r))?.snapshot || records[0].snapshot)
                            : "";
    }
    const signature =
        JSON.stringify(records.map((r) => [key(r.snapshot), stale(r), r.snapshot.paused]));
    if ($("worlds").dataset.signature !== signature + selectedWorld)
    {
        $("worlds").dataset.signature = signature + selectedWorld;
        $("worlds").replaceChildren(...records.map((r) => {
            const s = r.snapshot;
            const button = element("button", s.world_id);
            button.setAttribute("aria-pressed", String(key(s) === selectedWorld));
            button.append(element("small",
                                  `${s.game_id} · ${
                                      stale(r)   ? "오래된 정보"
                                      : s.paused ? "일시정지"
                                                 : "수신 중"}`));
            button.append(element("small", `실행 ${s.instance_id}`));
            button.onclick = () => {
                selectedWorld = key(s);
                selectedEntity = "";
                page = 0;
                render();
            };
            return button;
        }));
    }
    const record = current();
    $("empty").hidden = Boolean(record);
    $("content").hidden = !record;
    if (!record)
        return;
    const snapshot = record.snapshot;
    const epoch = selectedWorld + ":" + snapshot.epoch;
    if (selectedEpoch !== epoch)
    {
        selectedEntity = "";
        selectedEpoch = epoch;
    }
    $("world-title").textContent = snapshot.world_id;
    $("world-meta").textContent =
        `${snapshot.game_id} · 실행 ${snapshot.instance_id} · epoch ${snapshot.epoch} · 메시지 #${
            snapshot.sequence} · 월드 시간 ${format(snapshot.world_time_minutes)}분`;
    $("freshness").textContent = `${
        stale(record)     ? "오래된 정보"
        : snapshot.paused ? "일시정지 · 수신 중"
                          : "수신 중"} · ${Math.floor(age(record))}초 전`;
    $("freshness").className = "badge" + (stale(record) ? " warning" : "");
    $("cards").replaceChildren(
        ...[...Object.keys(kinds).filter((k) => k !== "player").map((kind) => {
            const card = element("div", undefined, "card");
            card.append(element("span", kinds[kind], "muted"),
                        element("strong", count(record.summary, kind)),
                        element("small", "생존 / 전체"));
            return card;
        }),
            (() => {
                const card = element("div", undefined, "card");
                card.append(element("span", "마을", "muted"),
                            element("strong", snapshot.settlements.length),
                            element("small", `플레이어 ${count(record.summary, "player")}`));
                return card;
            })()]);
    const openedTowns = new Set(Array.from($("settlements").querySelectorAll("details[open]"))
                                    .map((node) => node.dataset.town));
    $("settlements").replaceChildren(...snapshot.settlements.map((town) => {
        const panel = element("article", undefined, "town");
        const summary = record.settlements[town.id], m = town.metrics || {};
        panel.append(
            element("h3", town.name),
            element("p", `NPC ${count(summary, "npc")} · 몹 ${count(summary, "monster")}`));
        panel.append(element("p",
                             `위급 NPC ${summary.npc_critical} · 적대 NPC ${
                                 summary.npc_hostile} · 항복 ${summary.npc_surrendered}`));
        panel.append(element(
            "p",
            `주민 보유량 합계: 식량 ${format(m.resident_food_total)} / 약품 ${
                format(m.resident_medicine_total)} / 화폐 ${format(m.resident_coins_total)}`,
            "muted"));
        panel.append(element("p", "마을 경제·치안 지수: 미수집", "muted"));
        const more = element("details"), pre = element("pre", JSON.stringify(m, null, 2));
        more.dataset.town = town.id;
        more.open = openedTowns.has(town.id);
        more.append(element("summary", "마을 원본 지표"), pre);
        panel.append(more);
        return panel;
    }));
    const nextSignature =
        selectedWorld + JSON.stringify(snapshot.settlements.map((t) => [t.id, t.name]));
    if (townSignature !== nextSignature)
    {
        const old = $("town").value;
        $("town").replaceChildren(new Option("전체 마을", ""),
                                  ...snapshot.settlements.map((t) => new Option(t.name, t.id)));
        $("town").value = snapshot.settlements.some((t) => t.id === old) ? old : "";
        townSignature = nextSignature;
    }
    renderEntities();
}

function renderEntities()
{
    const record = current();
    if (!record)
        return;
    const focusedEntity = document.activeElement?.dataset.entity;
    const query = $("search").value.trim().toLowerCase(), kind = $("kind").value,
          town = $("town").value, life = $("life").value;
    const entities = record.snapshot.entities.filter(
        (e) => (!kind || e.kind === kind) && (!town || e.settlement_id === town) &&
               (!life || (life === "alive"  ? e.alive
                          : life === "dead" ? !e.alive
                                            : e.hostile && e.alive)) &&
               (!query || [ e.id, e.name, e.role, roles[e.role], e.action, e.action_label ]
                              .join(" ")
                              .toLowerCase()
                              .includes(query)));
    page = Math.max(0, Math.min(page, Math.ceil(entities.length / pageSize) - 1));
    $("count").textContent = `${entities.length}개 일치 · 이름을 선택해 상세 확인`;
    $("entities")
        .replaceChildren(...entities.slice(page * pageSize, (page + 1) * pageSize).map((e) => {
            const row = element("tr", undefined, e.id === selectedEntity ? "selected" : "");
            const name = element("td"), button = element("button", e.name || e.id);
            button.dataset.entity = e.id;
            button.onclick = () => {
                selectedEntity = e.id;
                renderEntities();
            };
            name.append(button, element("small", `#${e.id}`));
            row.append(name,
                       element("td", `${kinds[e.kind]} · ${roles[e.role] || e.role}`),
                       element("td", e.action_label || e.action),
                       element("td", `${format(e.health)} / ${format(e.max_health)}`),
                       element("td",
                               !e.alive    ? "사망"
                               : e.hostile ? "생존 · 적대"
                                           : "생존"),
                       element("td", `${format(e.position.x)}, ${format(e.position.y)}`));
            return row;
        }));
    if (!entities.length)
    {
        const row = element("tr"), cell = element("td", "조건에 맞는 개체가 없습니다.");
        cell.colSpan = 6;
        row.append(cell);
        $("entities").append(row);
    }
    if (focusedEntity)
    {
        Array.from($("entities").querySelectorAll("button"))
            .find((node) => node.dataset.entity === focusedEntity)
            ?.focus({preventScroll : true});
    }
    $("page").textContent = `${page + 1} / ${Math.max(1, Math.ceil(entities.length / pageSize))}`;
    $("prev").disabled = page === 0;
    $("next").disabled = (page + 1) * pageSize >= entities.length;
    renderDetail(record.snapshot.entities.find((e) => e.id === selectedEntity));
}

function renderDetail(entity)
{
    const detail = $("detail");
    if (!entity)
    {
        detail.textContent =
            selectedEntity ? "개체가 현재 스냅샷에서 제거되었습니다." : "선택된 개체가 없습니다.";
        return;
    }
    const rawOpen = detail.querySelector("details")?.open || false;
    const grid = element("div", undefined, "detail-grid");
    const groups = [
        [
            entity.name,
            {
                "Actor ID" : entity.id,
                "행동 코드" : entity.action,
                "행동" : entity.action_label,
                "대상 ID" : entity.target_id,
                "활성" : entity.active ? "예" : "아니요",
                "표시 플래그" : entity.visible ? "예" : "아니요"
            }
        ],
        [ "상태 · 인벤토리", entity.metrics || {} ],
        [ "성격", entity.personality || {} ]
    ];
    for (const [title, values] of groups)
    {
        const section = element("section"), list = element("dl");
        section.append(element("h3", title));
        for (const [name, value] of Object.entries(values))
            list.append(element("dt", labels[name] || name), element("dd", format(value)));
        section.append(Object.keys(values).length ? list : element("p", "미수집", "muted"));
        grid.append(section);
    }
    const raw = element("details");
    raw.open = rawOpen;
    raw.append(element("summary", "원본 JSON"), element("pre", JSON.stringify(entity, null, 2)));
    detail.replaceChildren(grid, raw);
}

async function poll()
{
    const controller = new AbortController(), timeout = setTimeout(() => controller.abort(), 2500);
    try
    {
        const response =
            await fetch("/api/state", {cache : "no-store", signal : controller.signal});
        if (!response.ok)
            throw new Error(`HTTP ${response.status}`);
        const state = await response.json();
        if (state.schema !== "world-monitor.state.v1" || !Array.isArray(state.worlds))
            throw new Error("규격 불일치");
        records = state.worlds;
        lastFetch = performance.now();
        receiverOnline = true;
        $("connection").textContent = "● 로컬 수신기 연결됨";
        $("connection").className = "badge";
    }
    catch (error)
    {
        receiverOnline = false;
        $("connection").textContent =
            `수신기 연결 끊김 · ${error.name === "AbortError" ? "시간 초과" : error.message}`;
        $("connection").className = "badge danger";
    }
    finally
    {
        clearTimeout(timeout);
        render();
        setTimeout(poll, document.hidden ? 3000 : 1000);
    }
}

for (const id of ["search", "kind", "town", "life"])
    $(id).addEventListener("input", () => {
        page = 0;
        renderEntities();
    });
$("prev").onclick = () => {
    page--;
    renderEntities();
};
$("next").onclick = () => {
    page++;
    renderEntities();
};
poll();
