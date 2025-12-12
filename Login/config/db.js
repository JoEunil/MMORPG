const mysql = require('mysql2');
require('dotenv').config(); 

// MySQL 연결 풀 생성
const pool = mysql.createPool({
  host: process.env.DB_HOST,
  user: process.env.DB_USER,
  password: process.env.DB_PW,
  database: process.env.DB_DB,
  waitForConnections: true,
  connectionLimit: 10, 
  queueLimit: 0 
});

// 연결 테스트
pool.getConnection((err, connection) => {
    if (err) {
        console.error('DB connection failed:', err);
    } else {
        console.log('DB connected!');
        connection.release(); // 연결 해제
    }
});

// 연결 풀을 export
module.exports = pool;