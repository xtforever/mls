-- ProxySQL setup for the mdb_srv multiplexing test.
--
--   Frontend (input):  up to 10000 client connections
--   Backend  (MariaDB): pool capped at 300 connections
--
-- Apply against the ProxySQL admin interface (default 127.0.0.1:6032):
--   mysql -h127.0.0.1 -P6032 -uadmin -padmin < proxysql_setup.sql
--
-- The MariaDB server must also allow at least the backend cap:
--   sudo mariadb -e "SET GLOBAL max_connections=500;"

UPDATE global_variables SET variable_value='10000'
  WHERE variable_name='mysql-max_connections';
UPDATE global_variables SET variable_value='mdb_demo'
  WHERE variable_name='mysql-monitor_username';
UPDATE global_variables SET variable_value='mdb_demo_pw'
  WHERE variable_name='mysql-monitor_password';

DELETE FROM mysql_servers;
INSERT INTO mysql_servers (hostgroup_id, hostname, port, max_connections, comment)
  VALUES (0, '127.0.0.1', 3306, 300, 'mariadb mdb_demo');

DELETE FROM mysql_users;
INSERT INTO mysql_users (username, password, default_hostgroup, active)
  VALUES ('mdb_demo', 'mdb_demo_pw', 0, 1);

LOAD MYSQL VARIABLES TO RUNTIME;
LOAD MYSQL SERVERS TO RUNTIME;
LOAD MYSQL USERS TO RUNTIME;
SAVE MYSQL VARIABLES TO DISK;
SAVE MYSQL SERVERS TO DISK;
SAVE MYSQL USERS TO DISK;
