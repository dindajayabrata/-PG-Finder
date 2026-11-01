-- EXISTING DATABASE ONLY: run once using F5 in the same schema as server.js.
-- This advances sequences; it does not delete any records.
DECLARE
    v_max_id NUMBER;
    v_next_id NUMBER;
BEGIN
    SELECT NVL(MAX(user_id), 0) INTO v_max_id FROM users;
    LOOP
        SELECT seq_user_id.NEXTVAL INTO v_next_id FROM DUAL;
        EXIT WHEN v_next_id > v_max_id;
    END LOOP;

    SELECT NVL(MAX(room_id), 0) INTO v_max_id FROM rooms;
    LOOP
        SELECT seq_room_id.NEXTVAL INTO v_next_id FROM DUAL;
        EXIT WHEN v_next_id > v_max_id;
    END LOOP;

    SELECT NVL(MAX(favorite_id), 0) INTO v_max_id FROM favorites;
    LOOP
        SELECT seq_favorite_id.NEXTVAL INTO v_next_id FROM DUAL;
        EXIT WHEN v_next_id > v_max_id;
    END LOOP;

    SELECT NVL(MAX(contact_id), 0) INTO v_max_id FROM contact_messages;
    LOOP
        SELECT seq_contact_id.NEXTVAL INTO v_next_id FROM DUAL;
        EXIT WHEN v_next_id > v_max_id;
    END LOOP;

END;
/
