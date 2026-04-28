const express = require('express');
const router = express.Router();
const db = require('../config/db');
const redis = require('../config/redis');
const { v4: uuidv4 } = require('uuid');


const jwt = require('jsonwebtoken');
const bcrypt = require('bcryptjs');
const authenticateToken = require('../middleware/authenticateToken'); 

require('dotenv').config(); 

// 로그인 요청 처리
router.post('/login', (req, res) => {
    const { login_id, password } = req.body;
  console.log(req.body);
    const query = 'SELECT * FROM users WHERE login_id = ?';
    const logQuery = 'INSERT INTO login_log (login_id, succeed, ip_addr, user_agent) VALUES (?, ?, ?, ?)';
    const ip_addr = req.headers['x-forwarded-for'] || req.socket.remoteAddress;
    const user_agent = req.headers['user-agent'] || null;

    db.execute(query, [login_id], (err, results) => {
      if (err) {
        console.error('데이터베이스 쿼리 오류:', err);
        return res.status(500).send('서버 오류');
      }

      if (results.length === 0) {
          db.execute(logQuery, [login_id, false, ip_addr, user_agent], () => { });
        return res.status(400).send('사용자가 존재하지 않습니다.');
      }

      // DB에서 찾은 사용자
      const user = results[0];

        // 비밀번호 비교
      const isMatch = bcrypt.compareSync(password, user.password);
      if (!isMatch) {
          db.execute(logQuery, [login_id, false, ip_addr, user_agent], () => {});
          return res.status(400).send('비밀번호가 일치하지 않습니다.');
      }
      db.execute(logQuery, [login_id, true, ip_addr, user_agent], () => { });

      // JWT 토큰 생성 (사용자 정보와 비밀 키를 기반으로 생성)
      const payload = {
        userId: user.user_id,
        login_id: user.login_id,
        email: user.email,
      };

      // 토큰 발급 (Access Token)
      const accessToken = jwt.sign(payload, process.env.JWT_SECRET, { expiresIn: '3h' });

      // Access Token 반환
      res.json({
          success: true,
          UserID : user.user_id,
          AccessToken: accessToken
      });
  });
});

// 회원가입
router.post('/signup', (req, res) => {
    const { login_id, password, email, phone_number} = req.body;
  console.log(req.body);
  // 사용자 정보가 없으면 에러 처리
  if (!login_id || !password || !email || !phone_number) {
    return res.status(400).send('모든 필드를 입력해야 합니다.');
  }

  // 비밀번호 해싱
  bcrypt.hash(password, 10, (err, hashedPassword) => {
    if (err) {
      console.error('비밀번호 해싱 실패:', err);
      return res.status(500).send('서버 오류');
    }
    // 회원가입된 사용자 정보 데이터베이스에 저장
    const query = 'INSERT INTO users (login_id, password, email, phone_number) VALUES (?, ?, ?, ?)';

    db.execute(query, [login_id, hashedPassword, email, phone_number], (err, results) => {
      if (err) {
        console.error('사용자 생성 오류:', err);
        return res.status(500).send('서버 오류');
      }
      return res.status(201).send('회원가입 성공');
    });
  });
});

router.post('/getSession', authenticateToken, async (req, res) => {
  try {
    console.log('Authenticated user:', req.user);

    // 게임 서버용 임시 세션 토큰 생성 
    const sessionToken = uuidv4();
    console.log('Generated session token:', sessionToken);
    // Redis에 세션 토큰 저장 (10분 동안 유효)
    await redis.set(req.user.userId.toString(), sessionToken, 'EX', 600);

    console.log('Session token saved in Redis');
    
    // 게임 서버 정보와 세션 토큰 반환
    res.json({
      userId: req.user.userId,   // <- 추가
      sessionToken, // 게임 서버용 세션 토큰
      gameServer: {
        ip: "127.0.0.1",
        port: 9999,
      }
    });
  } catch (err) {
    console.error('Redis error:', err);
    return res.status(500).json({ error: "Redis error", details: err });
  }
});

module.exports = router;