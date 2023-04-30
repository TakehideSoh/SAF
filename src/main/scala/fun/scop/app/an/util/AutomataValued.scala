package fun.scop.app.an.util

case class AutomataValued(a: Automata, v: Int) {
  def conflictWith(v0: AutomataValued) = {
    val result = (v0.a.name == a.name) && (v0.v != v)
    // println(this + " " + v0 + s": $result")

    result
  }

  override def toString = s"${a.name}=$v"
}
