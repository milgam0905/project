-- EXAMPLE CONTRACT ONLY. Do not substitute invented WBG rows.
-- CAST numeric source IDs to TEXT. payload preserves original rule content.
SELECT CAST(id AS TEXT), name, payload FROM wbg ORDER BY id;
