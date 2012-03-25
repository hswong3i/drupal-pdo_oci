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

-- $schema['test_people'].
CREATE TABLE test_people (
  name VARCHAR2(255) DEFAULT '' NOT NULL,
  age INT DEFAULT 0 NOT NULL CHECK (age >= 0),
  job VARCHAR2(255) DEFAULT '' NOT NULL,
  CONSTRAINT test_pk PRIMARY KEY (job),
  CONSTRAINT test_name_key UNIQUE (name)
);

CREATE INDEX test_people_ages_idx ON test_people (age);

INSERT INTO test (name, age, job) VALUES ('Meredith', 30, 'Speaker');

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

-- $schema['test_two_blobs']
CREATE TABLE test_two_blobs (
  id INT NOT NULL CHECK (id >= 0),
  blob1 BLOB DEFAULT EMPTY_BLOB(),
  blob2 BLOB DEFAULT EMPTY_BLOB(),
  CONSTRAINT test_two_blobs_pk PRIMARY KEY (id)
);

CREATE SEQUENCE test_two_blobs_id_seq MINVALUE 1 INCREMENT BY 1 START WITH 1 NOCACHE NOORDER NOCYCLE;

CREATE OR REPLACE TRIGGER test_two_blobs_id_tgr
  BEFORE INSERT ON test_two_blobs
  FOR EACH ROW
  WHEN (NEW.id IS NULL)
  BEGIN
    SELECT test_two_blobs_id_seq.NEXTVAL
    INTO :NEW.id
    FROM DUAL;
  END;
/

-- $schema['test_task'].
CREATE TABLE test_task (
  tid INT NOT NULL CHECK (tid >= 0),
  pid INT DEFAULT 0 NOT NULL CHECK (pid >= 0),
  task VARCHAR2(255) DEFAULT '' NOT NULL,
  priority INT DEFAULT 0 NOT NULL CHECK (priority >= 0),
  CONSTRAINT test_task_pk PRIMARY KEY (tid)
);

CREATE SEQUENCE test_task_tid_seq MINVALUE 1 INCREMENT BY 1 START WITH 1 NOCACHE NOORDER NOCYCLE;

CREATE OR REPLACE TRIGGER test_task_tid_tgr
  BEFORE INSERT ON test_task
  FOR EACH ROW
  WHEN (NEW.tid IS NULL)
  BEGIN
    SELECT test_task_tid_seq.NEXTVAL
    INTO :NEW.tid
    FROM DUAL;
  END;
/

INSERT INTO test_task (pid, task, priority) VALUES (1, 'eat', 3);
INSERT INTO test_task (pid, task, priority) VALUES (1, 'sleep', 4);
INSERT INTO test_task (pid, task, priority) VALUES (1, 'code', 1);
INSERT INTO test_task (pid, task, priority) VALUES (2, 'sing', 2);
INSERT INTO test_task (pid, task, priority) VALUES (2, 'sleep', 2);
INSERT INTO test_task (pid, task, priority) VALUES (4, 'found new band', 1);
INSERT INTO test_task (pid, task, priority) VALUES (4, 'perform at superbowl', 3);

EXIT;
