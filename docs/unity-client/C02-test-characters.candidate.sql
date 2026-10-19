-- C02-DB-01 / r1: approval candidate only. NOT EXECUTED.
-- Use one connection. Driver must abort/ROLLBACK on any error or unexpected row count.
-- No DDL, seeding procedure, existing row UPDATE, currency, or inventory write.
START TRANSACTION;
SELECT user_id, login_id FROM login.users
WHERE login_id IN ('unity_ai_01', 'unity_ai_02') FOR UPDATE;
SET @c02_u1 = (SELECT user_id FROM login.users WHERE login_id='unity_ai_01');
SET @c02_u2 = (SELECT user_id FROM login.users WHERE login_id='unity_ai_02');
SET @c02_ready = (@c02_u1 IS NOT NULL AND @c02_u2 IS NOT NULL AND @c02_u1 <> @c02_u2
 AND NOT EXISTS (SELECT 1 FROM game.characters WHERE user_id IN (@c02_u1,@c02_u2))
 AND NOT EXISTS (SELECT 1 FROM game.profile WHERE name IN ('unity_ai_01_c02','unity_ai_02_c02')));
SELECT @c02_ready AS must_equal_1;
-- Driver requires ready=1 before continuing; both users must be offline.
INSERT INTO game.profile(name) SELECT 'unity_ai_01_c02' WHERE @c02_ready;
-- Driver requires affected_rows=1.
SET @c02_p1=LAST_INSERT_ID();
INSERT INTO game.characters(user_id,channel_id,profile_id,zone_id,last_pos_x,last_pos_y)
SELECT @c02_u1,1,@c02_p1,1,10,10 WHERE @c02_ready;
-- Driver requires affected_rows=1.
SET @c02_c1=LAST_INSERT_ID();
INSERT INTO game.profile(name) SELECT 'unity_ai_02_c02' WHERE @c02_ready;
-- Driver requires affected_rows=1.
SET @c02_p2=LAST_INSERT_ID();
INSERT INTO game.characters(user_id,channel_id,profile_id,zone_id,last_pos_x,last_pos_y)
SELECT @c02_u2,1,@c02_p2,1,12,10 WHERE @c02_ready;
-- Driver requires affected_rows=1.
SET @c02_c2=LAST_INSERT_ID();
SELECT u.login_id,c.char_id,c.user_id,c.channel_id,c.profile_id,p.name,p.version,
 c.zone_id,c.last_pos_x,c.last_pos_y,c.level,c.exp,c.hp,c.max_hp,c.mp,c.max_mp,c.attack,c.dir
FROM login.users u JOIN game.characters c ON c.user_id=u.user_id
JOIN game.profile p ON p.profile_id=c.profile_id
WHERE (c.char_id=@c02_c1 AND c.profile_id=@c02_p1 AND u.login_id='unity_ai_01')
 OR (c.char_id=@c02_c2 AND c.profile_id=@c02_p2 AND u.login_id='unity_ai_02');
-- Driver requires exactly the two intended rows and expected values; record only these IDs.
-- COMMIT only after approved execution and all assertions. Default inspection ends with ROLLBACK.
ROLLBACK;
