using ClientCore;
using System;
using System.Xml.Serialization;
using TMPro;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class MyHeroControl : MonoBehaviour
{
    // 서버 이동 예산(MOVE_BUDGET_PER_TICK = 1.0 / 50ms = 20 units/sec)보다 낮게 잡아
    // 정상 이동이 속도 검증에 걸리지 않도록 한다.
    private const float MOVE_SPEED = 10f;
    // 서버 좌표와 이만큼 벌어지면 예측이 틀린 것(이동 거부·텔레포트)으로 보고 스냅한다.
    // 이동 중 서버 좌표는 (RTT + 틱)만큼 뒤처지는 게 정상이므로, 그 지연분보다 넉넉하게 잡아야
    // 정상 이동을 거부로 오인해 스냅하지 않는다. 10 units/sec 기준 200ms 지연 = 2.0.
    private const float RECONCILE_SNAP = 3.0f;

    private Animator animator;
    private SpriteRenderer spriteRenderer;
    [SerializeField] private TextMeshProUGUI Name;
    [SerializeField] private TextMeshProUGUI Level;
    [SerializeField] private TestUI testUI;
    [SerializeField] private CameraFollow cam;
    [SerializeField] private ZoneChageTrigger trigger;

    private Vector2 _moveInput;
    private int _inputDirection;
    private IViewDataUI _viewData;

    private Vector3 _predictedPos; // 클라이언트가 로컬로 굴리는 좌표 — 이 값을 서버에 보낸다
    private Vector3 _serverPos;    // 서버가 확정해 내려준 좌표
    private int _direction;
    private bool _isMoving;
    void Start()
    {
        animator = GetComponent<Animator>();
        spriteRenderer = GetComponent<SpriteRenderer>();
        _viewData = CoreManager.Instance.VD;
    }

    void Update()
    {
        Vector3 delta = Vector3.zero;

        if (!IsTyping())
        {
            if (Input.GetKeyDown(KeyCode.Z))
            {
                _viewData.UpdateSkill(0);
            }
            else if (Input.GetKeyDown(KeyCode.X))
            {
                _viewData.UpdateSkill(1);
            }
            else if (Input.GetKeyDown(KeyCode.C))
            {
                _viewData.UpdateSkill(2);
            }

            _moveInput.x = Input.GetAxisRaw("Horizontal");
            _moveInput.y = Input.GetAxisRaw("Vertical");

            if (_moveInput != Vector2.zero)
            {
                if (_moveInput.x != 0)
                {
                    _moveInput.y = 0;
                    _inputDirection = _moveInput.x > 0 ? 3 : 2;
                }
                else if (_moveInput.y != 0)
                {
                    _inputDirection = _moveInput.y > 0 ? 0 : 1;
                }
                // 로컬 예측 이동 — 입력 즉시 반응한다. 서버 응답을 기다리지 않는다.
                delta = new Vector3(_moveInput.x, _moveInput.y, 0f) * (MOVE_SPEED * Time.deltaTime);
                _direction = _inputDirection;
            }
        }

        if (delta != Vector3.zero)
            _predictedPos += delta;

        // 서버 보정 — 이동 중 서버 좌표가 뒤처지는 것은 네트워크 지연에 의한 정상 상태다.
        // 매 프레임 그쪽으로 끌어당기면 앞으로 가는 예측을 계속 뒤로 잡아당겨 러버밴딩이 보인다.
        // 서버는 클라이언트가 보낸 좌표를 그대로 받아들이므로 이동을 멈추면 자연히 수렴한다.
        // 따라서 서버가 실제로 이동을 거부했거나 텔레포트된 경우(= 큰 격차)에만 스냅한다.
        float driftSq = (_serverPos - _predictedPos).sqrMagnitude;
        if (driftSq > RECONCILE_SNAP * RECONCILE_SNAP)
            _predictedPos = _serverPos;

        transform.position = _predictedPos;
        _isMoving = delta != Vector3.zero;

        animator.SetBool("IsMoving", _isMoving);
        if (_isMoving)
        {
            if (_direction == 3)
            {
                animator.SetFloat("Direction", 2);
            }
            else
            {
                animator.SetFloat("Direction", _direction);
            }

            spriteRenderer.flipX = (_direction == 2);

            // 최종 좌표를 기록만 한다. 실제 송신은 ClientTick이 틱당 1회 수행하므로
            // 프레임레이트가 높아도 서버로 나가는 입력 수는 늘지 않는다.
            _viewData.UpdateMove((byte)_inputDirection, _predictedPos.x, _predictedPos.y);
        }
    }
    public void Teleport(byte direction, float x, float y)
    {
        var pos = new Vector3(x, y, transform.position.z);
        transform.position = pos;
        _predictedPos = pos;   // 예측도 같이 리셋해야 보정 로직이 되돌리지 않는다
        _serverPos = pos;
        _direction = direction;
        if (cam != null)
        {
            cam.InstantSnap();
        }
    }
    // 서버 스냅샷이 내려준 권위 좌표. 예측과의 차이는 Update의 보정 단계에서 흡수한다.
    public void UpdatePosition(byte direction, float x, float y)
    {
        _serverPos = new Vector3(x, y, transform.position.z);
        _direction = direction;
        testUI.SetPosition(x, y);
        trigger.CheckZoneChange(x, y);
    }
    public void SetNameLevel(string name, ushort level)
    {
        Name.text = name;
        Level.text = "Lv." + level;
    }
    public void UpdateHPMP(int HP, int MP)
    {

    }
    private bool IsTyping()
    {
        var selected = EventSystem.current.currentSelectedGameObject;
        return selected != null &&
               (selected.GetComponent<UnityEngine.UI.InputField>() != null ||
                selected.GetComponent<TMPro.TMP_InputField>() != null);
    }
    public void StartSkill(byte dir, uint skillId)
    {
        animator.SetFloat("Direction", _direction);
        animator.SetTrigger("Attack");
    }
}