
c---------------------------------------------------------------------
c---------------------------------------------------------------------

       subroutine  add

c---------------------------------------------------------------------
c---------------------------------------------------------------------

c---------------------------------------------------------------------
c addition of update to the vector u
c---------------------------------------------------------------------

       include 'header.h'

       integer i,j,k,m

       do k = 1, nz2
          do j = 1, ny2
             do i = 1, nx2
                do m = 1, 5
                   u(m,i,j,k) = u(m,i,j,k) + rhs(m,i,j,k)
                end do
             end do
          end do
       end do

       return
       end

