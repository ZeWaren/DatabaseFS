CREATE TABLE dirtbl (
  uname varchar(128) NOT NULL default '',
  dirid int(6) NOT NULL default '0',
  template varchar(128) NOT NULL default '',
  subptr int(6) default NULL,
  filesql text,
  dirsql text,
  priority int(3) default NULL,
  visible char(1) default NULL,
  PRIMARY KEY  (uname,dirid,template)
) TYPE=MyISAM;
INSERT INTO dirtbl VALUES ('*', 1,'./',1,NULL,NULL,1,'F');

CREATE TABLE logs (
  uname varchar(128) NOT NULL default '',
  accesstm datetime NOT NULL default '0000-00-00 00:00:00',
  SQL text
) TYPE=MyISAM;
