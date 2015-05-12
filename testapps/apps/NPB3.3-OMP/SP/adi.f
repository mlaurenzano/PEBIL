
c---------------------------------------------------------------------
c---------------------------------------------------------------------

       subroutine  adi

       include 'header.h'

c---------------------------------------------------------------------
c---------------------------------------------------------------------

       if (timeron) call timer_start(t_rhs)
       call compute_rhs
       if (timeron) call timer_stop(t_rhs)

       if (timeron) call timer_start(t_txinvr)
       call txinvr
       if (timeron) call timer_stop(t_txinvr)

       call x_solve

       call y_solve

       call z_solve

       if (timeron) call timer_start(t_add)
       call add
       if (timeron) call timer_stop(t_add)

       return
       end

