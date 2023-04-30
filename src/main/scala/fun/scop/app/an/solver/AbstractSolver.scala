package fun.scop.app.an.solver

import fun.scop.app.an.util._

abstract class AbstractSolver() {
  def findAttractorsLeK(
      k: Int,
      attractorsFoundSoFar: Seq[Attractor]
  ): Seq[Attractor]
}
