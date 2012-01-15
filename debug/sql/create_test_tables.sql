/**
 * To login sqlplus:
 *   su - oracle
 *   sqlplus test/CHANGE
 */

CREATE TABLE test (
  id INT NOT NULL CHECK (id >= 0),
  name VARCHAR2(255) DEFAULT '' NOT NULL,
  age INT DEFAULT 0 NOT NULL CHECK (age >= 0),
  job VARCHAR2(255) DEFAULT 'Undefined' NOT NULL
);

CREATE SEQUENCE test_id_seq MINVALUE 1 INCREMENT BY 1 START WITH 1 NOCACHE NOORDER NOCYCLE;

CREATE OR REPLACE TRIGGER test_id_tgr
  BEFORE INSERT ON test
  FOR EACH ROW
  WHEN (NEW.id IS NULL)
  BEGIN
    SELECT test_id_seq.NEXTVAL
    INTO :NEW.id
    FROM DUAL;
  END;
/

CREATE TABLE test_one_blob (
  id INT NOT NULL CHECK (id >= 0),
  blob1 BLOB DEFAULT EMPTY_BLOB()
);

CREATE SEQUENCE test_one_blob_id_seq MINVALUE 1 INCREMENT BY 1 START WITH 1 NOCACHE NOORDER NOCYCLE;

CREATE OR REPLACE TRIGGER test_one_blob_id_tgr
  BEFORE INSERT ON test_one_blob
  FOR EACH ROW
  WHEN (NEW.id IS NULL)
  BEGIN
    SELECT test_one_blob_id_seq.NEXTVAL
    INTO :NEW.id
    FROM DUAL;
  END;
/

INSERT INTO test (name, age, job) VALUES ('John', 25, 'Singer');
INSERT INTO test (name, age, job) VALUES ('George', 27, 'Singer');
INSERT INTO test (name, age, job) VALUES ('Ringo', 28, 'Drummer');
INSERT INTO test (name, age, job) VALUES ('Paul', 26, 'Songwriter');
