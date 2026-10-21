-- EXAMPLE CONTRACT ONLY. Use UNION ALL to combine separate NPC/beast tables.
-- kind: npc|beast; role: civilian|bandit|knight|mage; IDs globally unique.
SELECT id, archetype_id, kind, role, region_id, home_region_id,
       power, mana, hunger, gold, food, alive
FROM actors ORDER BY id;
