using ClientCore;
using System;
using TMPro;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;
using System.Collections;

public class HeroControl : MonoBehaviour
{
    private Animator animator;
    private SpriteRenderer spriteRenderer;

    [SerializeField] private TextMeshProUGUI Name;
    [SerializeField] private TextMeshProUGUI Level;
    private Vector3 _targetPos;
    private int _direction;
    private bool _isMoving = true;
    private void Awake()
    {
        animator = GetComponent<Animator>();
        spriteRenderer = GetComponent<SpriteRenderer>();
    }
    void Start()
    {
        _targetPos = transform.position;
    }

    void Update()
    {
        if ((_targetPos - transform.position).sqrMagnitude > 0.001f)
        {
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

        spriteRenderer.flipX = (_direction == 2);
    }

    public void Teleport(byte direction, float x, float y)
    {
        transform.position = new Vector3(x, y, transform.position.z);
        _targetPos = new Vector3(x, y, transform.position.z);
        _direction = direction;
    }
    public void UpdatePosition(byte direction, float x, float y)
    {
        _targetPos = new Vector3(x, y, transform.position.z);
        _direction = direction;
    }
    public void SetNameLevel(string name, ushort level)
    {
        StartCoroutine(DelayedUpdateUI(name, level));
    }

    private IEnumerator DelayedUpdateUI(string name, ushort level)
    {
        yield return null; // 한 프레임 대기
        if (Name != null) Name.text = name;
        if (Level != null) Level.text = "Lv." + level;
        if (Name != null) Name.ForceMeshUpdate();
        if (Level != null) Level.ForceMeshUpdate();
    }
    public void UpdateHPMP(int HP, int MP, int MAXHP, int MAXMP)
    {

    }
    public void StartSkill(byte dir, uint skillId)
    {
        animator.SetFloat("Direction", _direction);
        animator.SetTrigger("Attack");
    }
}
