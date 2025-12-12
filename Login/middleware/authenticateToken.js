const jwt = require('jsonwebtoken');

const authenticateToken = (req, res, next) => {
  // 클라이언트로부터 Authorization 헤더로 전달된 토큰 가져오기
  const token = req.headers['authorization']?.split(' ')[1]; // "Bearer <token>"

  if (!token) {
    return res.status(401).json({ message: 'No token provided' });
  }

  // JWT 검증
  jwt.verify(token, process.env.JWT_SECRET, (err, decoded) => {
    if (err) {
      return res.status(401).json({ message: 'Invalid or expired token' });
    }
    const userInfo = {
        userId: decoded.userId,
        username: decoded.username,
        email: decoded.email,
    };
    req.user = userInfo;
    console.log('Authenticated user:', req.user);
    next(); 
  });
};
module.exports = authenticateToken;   