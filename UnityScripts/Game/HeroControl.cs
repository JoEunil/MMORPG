using System;
using TMPro;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

public class HeroControl : MonoBehaviour
{
    private Animator animator;
    private SpriteRenderer spriteRenderer;

    [SerializeField] private TextMeshProUGUI Name;
    [SerializeField] private TextMeshProUGUI Level;

    private Vector3 _targetPos;
    private int _direction;
    private bool _isMoving = true;

    void Start()
    {
        _targetPos = transform.position;
        animator = GetComponent<Animator>();
        spriteRenderer = GetComponent<SpriteRenderer>();
    }

    //void Update()
    //{

    //}
    //public void UpdatePosition(byte direction, float x, float y)
    //{
    //    transform.position = new Vector3(x, y, transform.position.z);
    //    _direction = direction;
    //}
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

    public void UpdatePosition(byte direction, float x, float y)
    {
        _targetPos = new Vector3(x, y, transform.position.z);
        _direction = direction;
    }
    public void SetNameLevel(string name, ushort level)
    {
        Name.text = name;
        Level.text = "Lv." + level;
    }
    public void UpdateHPMP(int HP, int MP)
    {

    }
}
