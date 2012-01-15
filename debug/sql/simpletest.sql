/**
 * To login sqlplus:
 *   su - oracle
 *   sqlplus SYS@AL32UTF8/CHANGE AS SYSDBA @simpletest.sql
 */

-- Connect with SYS and create new user "test".
CONNECT SYS@AL32UTF8/CHANGE AS SYSDBA

DROP USER test CASCADE;

CREATE USER test PROFILE "DEFAULT" IDENTIFIED BY "CHANGE" DEFAULT TABLESPACE "USERS" TEMPORARY TABLESPACE "TEMP" ACCOUNT UNLOCK;
GRANT "AQ_ADMINISTRATOR_ROLE" TO test;
GRANT "AQ_USER_ROLE" TO test;
GRANT "AUTHENTICATEDUSER" TO test;
GRANT "CONNECT" TO test;
GRANT "CTXAPP" TO test;
GRANT "DBA" TO test;
GRANT "DELETE_CATALOG_ROLE" TO test;
GRANT "EJBCLIENT" TO test;
GRANT "EXECUTE_CATALOG_ROLE" TO test;
GRANT "EXP_FULL_DATABASE" TO test;
GRANT "GATHER_SYSTEM_STATISTICS" TO test;

-- Connect as user "test".
CONNECT test@AL32UTF8/CHANGE

-- $schema['test'].
CREATE TABLE test (
  id INT NOT NULL CHECK (id >= 0),
  name VARCHAR2(255) DEFAULT '' NOT NULL,
  age INT DEFAULT 0 NOT NULL CHECK (age >= 0),
  job VARCHAR2(255) DEFAULT 'Undefined' NOT NULL,
  CONSTRAINT test_pk PRIMARY KEY (id),
  CONSTRAINT test_name_key UNIQUE (name)
);

CREATE INDEX test_ages_idx ON test (age);

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

INSERT INTO test (name, age, job) VALUES ('John', 25, 'Singer');
INSERT INTO test (name, age, job) VALUES ('George', 27, 'Singer');
INSERT INTO test (name, age, job) VALUES ('Ringo', 28, 'Drummer');
INSERT INTO test (name, age, job) VALUES ('Paul', 26, 'Songwriter');

-- $schema['test_one_blob']
CREATE TABLE test_one_blob (
  id INT NOT NULL CHECK (id >= 0),
  blob1 BLOB DEFAULT EMPTY_BLOB(),
  CONSTRAINT test_one_blob_pk PRIMARY KEY (id)
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

EXIT;
