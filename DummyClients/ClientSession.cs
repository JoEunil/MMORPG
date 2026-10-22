using ClientCore.Network;
using ClientCore.PacketHelper;
using System;

namespace ClientCore
{
    internal class ClientSession
    {
        private readonly object _lock = new object();

        private TCPSocket _sock;
        private ulong _userID;
        private string _accessToken;
        private string _sessionToken;

        // Dummy에 viewData가 없으므로, ClientSession에서 좌표를 관리한다.
        private float _x;
        private float _y;
        private float _authoritativeX;
        private float _authoritativeY;
        private bool _hasAuthoritativePosition;
        private bool _hasPosition;
        private ushort _zoneID;
        private ulong _zoneGeneration;
        private ulong _ownershipEpoch;
        private uint _zoneInternalID;
        private ulong _bootstrapID;
        private ulong _bootstrapGeneration;
        private bool _gotOwnBootstrapFull;
        private bool _gotPlayerBootstrapEnd;
        private bool _gotBootstrapMonsters;
        private bool _worldReady;
        private long _nextBootstrapResyncAt;
        private int _bootstrapResyncAttempts;
        private bool _readyCounted;
        private bool _bootstrapFailureReported;
        private const float MAXIMUM_INPUT_LEAD = 0.6f;

        public ClientSession(TCPSocket sock)
        {
            _sock = sock;
        }

        public void SetPosition(float x, float y)
        {
            lock (_lock) { _x = x; _y = y; _hasPosition = true; }
        }

        public bool HasPosition
        {
            get { lock (_lock) return _hasPosition; }
        }

        public uint OwnZoneInternalID
        {
            get { lock (_lock) return _zoneInternalID; }
        }

        public bool TryMarkReadyCounted()
        {
            lock (_lock)
            {
                if (!_worldReady || _readyCounted) return false;
                _readyCounted = true;
                return true;
            }
        }

        public byte[]? BuildMovePacket(byte dir, float step)
        {
            float x, y;
            ulong epoch;
            lock (_lock)
            {
                if (!_hasPosition || !_worldReady)
                    return null;
                if (DistanceFromAuthority() > MAXIMUM_INPUT_LEAD + 0.0001f)
                    return null;
                float nextX = _x, nextY = _y;
                switch (dir)
                {
                    case 0: nextY += step; break;
                    case 1: nextY -= step; break;
                    case 2: nextX -= step; break;
                    case 3: nextX += step; break;
                }
                float leadX = nextX - _authoritativeX, leadY = nextY - _authoritativeY;
                if (_hasAuthoritativePosition && leadX * leadX + leadY * leadY > MAXIMUM_INPUT_LEAD * MAXIMUM_INPUT_LEAD + 0.0001f)
                    return null;
                _x = nextX;
                _y = nextY;
                x = _x;
                y = _y;
                epoch = _ownershipEpoch;
            }
            return PacketBuilder.CreateActionPacketFast(dir, x, y, Config.NONE_SKILL, epoch);
        }

        public void SetZoneOwnership(ushort zoneID, ulong generation, ulong ownershipEpoch, uint ownID, float x, float y)
        {
            if (zoneID < 1 || zoneID > 6 || generation == 0 || ownershipEpoch == 0 || ownID == 0 || ownID == uint.MaxValue ||
                float.IsNaN(x) || float.IsInfinity(x) || float.IsNaN(y) || float.IsInfinity(y))
                return;
            lock (_lock)
            {
                if (_ownershipEpoch != 0 && ownershipEpoch <= _ownershipEpoch) return;
                _zoneID = zoneID;
                _zoneGeneration = generation;
                _ownershipEpoch = ownershipEpoch;
                _zoneInternalID = ownID;
                _bootstrapID = 0;
                _bootstrapGeneration = 0;
                _gotOwnBootstrapFull = false;
                _gotPlayerBootstrapEnd = false;
                _gotBootstrapMonsters = false;
                _worldReady = false;
                _bootstrapResyncAttempts = 0;
                _nextBootstrapResyncAt = Environment.TickCount64 + 2000;
                _bootstrapFailureReported = false;
                _x = x;
                _y = y;
                _authoritativeX = x;
                _authoritativeY = y;
                _hasAuthoritativePosition = true;
                _hasPosition = true;
            }
        }

        public void ObservePlayerFull(ushort zoneID, ulong generation, ulong epoch, ulong bootstrapID,
            bool bootstrapEnd, bool ownFound, float x, float y)
        {
            lock (_lock)
            {
                if (!_hasPosition || zoneID != _zoneID || generation < _zoneGeneration ||
                    float.IsNaN(x) || float.IsInfinity(x) || float.IsNaN(y) || float.IsInfinity(y)) return;
                if (epoch != 0 && _worldReady) return;
                if (epoch == 0 && bootstrapID == 0)
                {
                    if (_worldReady && ownFound)
                    {
                        _zoneGeneration = generation;
                        RebasePosition(x, y);
                    }
                    return;
                }
                if (epoch != _ownershipEpoch || bootstrapID == 0 || !AcceptBootstrapIdentity(generation, bootstrapID)) return;
                if (ownFound)
                {
                    _gotOwnBootstrapFull = true;
                    RebasePosition(x, y);
                }
                _gotPlayerBootstrapEnd |= bootstrapEnd;
                UpdateWorldReady();
            }
        }

        public void ObserveMonsterFull(ushort zoneID, ulong generation, ulong epoch, ulong bootstrapID, bool bootstrapEnd)
        {
            lock (_lock)
            {
                if (_worldReady || zoneID != _zoneID || generation < _zoneGeneration || epoch != _ownershipEpoch ||
                    bootstrapID == 0 || !AcceptBootstrapIdentity(generation, bootstrapID)) return;
                // A zero-count monster snapshot is valid when it terminates the matching bootstrap.
                _gotBootstrapMonsters |= bootstrapEnd;
                UpdateWorldReady();
            }
        }

        public void ObserveDelta(ushort zoneID, ulong generation, uint internalID, float? x, float? y)
        {
            lock (_lock)
            {
                if (!_worldReady || zoneID != _zoneID || generation < _zoneGeneration || internalID != _zoneInternalID) return;
                if (!x.HasValue && !y.HasValue) return;
                if (generation > _zoneGeneration) _zoneGeneration = generation;
                RebasePosition(x ?? _authoritativeX, y ?? _authoritativeY);
            }
        }

        public byte[]? BuildBootstrapResyncPacket(long now)
        {
            lock (_lock)
            {
                if (_worldReady || _ownershipEpoch == 0 || _bootstrapResyncAttempts >= 3 || now < _nextBootstrapResyncAt) return null;
                _bootstrapResyncAttempts++;
                _nextBootstrapResyncAt = now + 2000;
                // Allow the resent current-epoch pair to establish a fresh matching bootstrap ID.
                _bootstrapID = 0;
                _bootstrapGeneration = 0;
                _gotOwnBootstrapFull = false;
                _gotPlayerBootstrapEnd = false;
                _gotBootstrapMonsters = false;
                return PacketBuilder.CreateZoneResyncPacket(_ownershipEpoch);
            }
        }

        public bool TakeBootstrapFailureReport(long now)
        {
            lock (_lock)
            {
                if (_worldReady || _ownershipEpoch == 0 || _bootstrapResyncAttempts < 3 ||
                    now < _nextBootstrapResyncAt || _bootstrapFailureReported) return false;
                _bootstrapFailureReported = true;
                return true;
            }
        }

        private void UpdateWorldReady()
        {
            if (_bootstrapID == 0 || !_gotOwnBootstrapFull || !_gotPlayerBootstrapEnd || !_gotBootstrapMonsters) return;
            _zoneGeneration = _bootstrapGeneration;
            _worldReady = true;
        }

        private bool AcceptBootstrapIdentity(ulong generation, ulong bootstrapID)
        {
            if (generation < _zoneGeneration || (_bootstrapID != 0 &&
                (bootstrapID < _bootstrapID || generation < _bootstrapGeneration))) return false;
            if (_bootstrapID == 0 || bootstrapID > _bootstrapID || generation > _bootstrapGeneration)
            {
                _bootstrapID = bootstrapID;
                _bootstrapGeneration = generation;
                _gotOwnBootstrapFull = false;
                _gotPlayerBootstrapEnd = false;
                _gotBootstrapMonsters = false;
            }
            return bootstrapID == _bootstrapID && generation == _bootstrapGeneration;
        }

        private void RebasePosition(float x, float y)
        {
            if (float.IsNaN(x) || float.IsInfinity(x) || float.IsNaN(y) || float.IsInfinity(y)) return;
            _authoritativeX = _x = x;
            _authoritativeY = _y = y;
            _hasAuthoritativePosition = true;
            _hasPosition = true;
        }

        private float DistanceFromAuthority()
        {
            if (!_hasAuthoritativePosition) return 0;
            float dx = _x - _authoritativeX, dy = _y - _authoritativeY;
            return MathF.Sqrt(dx * dx + dy * dy);
        }
        public TCPSocket GetSocket()
        {
            return _sock;
        }
        public ulong UserID
        {
            get { lock (_lock) return _userID; }
            set { lock (_lock) _userID = value; }
        }

        public bool HasAccessToken
        {
            get { lock (_lock) return !string.IsNullOrEmpty(_accessToken); }
        }

        public bool HasSessionToken
        {
            get { lock (_lock) return !string.IsNullOrEmpty(_sessionToken); }
        }

        public string AccessToken
        {
            get { lock (_lock) return _accessToken; }
            set { lock (_lock) { _accessToken = value; } }
        }

        public string SessionToken
        {
            get { lock (_lock) return _sessionToken; }
            set { lock (_lock) { _sessionToken = value; } }
        }
    }
}
