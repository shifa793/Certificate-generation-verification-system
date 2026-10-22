CREATE DATABASE IF NOT EXISTS certificate_system CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE certificate_system;

CREATE TABLE IF NOT EXISTS certificates (
  certificate_id VARCHAR(50) PRIMARY KEY,
  student_name VARCHAR(100) NOT NULL,
  course_name VARCHAR(150) NOT NULL,
  issue_date DATE NOT NULL,
  authorized_name VARCHAR(100) NOT NULL,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
