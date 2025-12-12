using ClientCore;
using System;
using TMPro;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class MyHeroControl : MonoBehaviour
{
    private float moveSpeed = 0.5f;
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

    private Vector3 _targetPos;
    private int _direction;
    private bool _isMoving;
    void Start()
    {
        animator = GetComponent<Animator>();
        spriteRenderer = GetComponent<SpriteRenderer>();
        _viewData = CoreManager.Instance.VD;
    }

    //void Update()
    //{
    //    if (IsTyping()) return;

    //    _moveInput.x = Input.GetAxisRaw("Horizontal");
    //    _moveInput.y = Input.GetAxisRaw("Vertical");

    //    if (_moveInput != Vector2.zero)
    //    {
    //        if (_moveInput.x != 0)
    //        {
    //            _moveInput.y = 0;
    //            _inputDirection = _moveInput.x > 0 ? 3 : 2;
    //        }
    //        else if (_moveInput.y != 0)
    //        {
    //            _inputDirection = _moveInput.y > 0 ? 0 : 1;
    //        }
    //        _viewData.UpdateMove((byte)_inputDirection, moveSpeed);
    //    }
    //}
    //public void Teleport(byte direction, float x, float y)
    //{
    //    transform.position = new Vector3(x, y, transform.position.z);
    //    _direction = direction;
    //    CameraFollow cam = Camera.main.GetComponent<CameraFollow>();
    //    if (cam != null)
    //    {
    //        cam.InstantSnap();
    //    }
    //}
    //public void UpdatePosition(byte direction, float x, float y)
    //{
    //    transform.position = new Vector3(x, y, transform.position.z);
    //    _direction = direction;
    //    testUI.SetPosition(x, y);
    //}
    void Update()
    {
        if ((_targetPos - transform.position).sqrMagnitude > 0.001f)
        {
            // speed == 0.5f per frame -> 10f per second; (20fps)
            transform.position = Vector3.MoveTowards(transform.position, _targetPos, 10f * Time.deltaTime);
            _isMoving = true;
        }
        else
        {
            transform.position = _targetPos;
            _isMoving = false;
        }

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
        }


        if (IsTyping()) return;

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
            _viewData.UpdateMove((byte)_inputDirection, moveSpeed);
        }

    }
    public void Teleport(byte direction, float x, float y)
    {
        transform.position = new Vector3(x, y, transform.position.z);
        _targetPos = new Vector3(x, y, transform.position.z);
        _direction = direction;
        if (cam != null)
        {
            cam.InstantSnap();
        }
    }
    public void UpdatePosition(byte direction, float x, float y)
    {
        _targetPos = new Vector3(x, y, transform.position.z);
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
}